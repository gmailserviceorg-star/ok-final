#include <windows.h>
#include <winioctl.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include "payload.h"

using namespace std;

static const char* GRUB_CFG = R"CFG(set timeout=10
set default=0
insmod all_video
set gfxpayload=keep
menuentry "Windows" {
  search --no-floppy --file --set=root /bootmgr
  chainloader +1
}
menuentry "ClaudeOS" {
  search --no-floppy --file --set=root /claudeos/boot/claudeos.elf
  multiboot /claudeos/boot/claudeos.elf
  boot
}
)CFG";

struct Disk { int n; string name; uint64_t size; string style; int parts; bool sys; };
static bool dry = false;

static string ps(const string& script) {
  string c = "powershell -NoProfile -ExecutionPolicy Bypass -Command \"" + script + "\" 2>&1";
  FILE* f = _popen(c.c_str(), "r"); string out; if (!f) return out;
  char b[512]; while (fgets(b, sizeof b, f)) out += b; _pclose(f); return out;
}
static vector<string> split(const string& s, char d) { vector<string> v; string t; for (char c : s) { if (c == d) { v.push_back(t); t.clear(); } else if (c != '\r' && c != '\n') t += c; } v.push_back(t); return v; }
static string gb(uint64_t b) { char t[32]; snprintf(t, 32, "%.1f GB", b / 1073741824.0); return t; }
static string ask(const string& q) { cout << q; cout.flush(); string s; getline(cin, s); return s; }
static void pause_() { ask("\nPress Enter to close..."); }
static int fail(const string& m) { cout << "\n[ERROR] " << m << "\nNothing further was changed.\n"; pause_(); return 1; }

static int firmware() {  // 1 = legacy BIOS, 2 = UEFI, 0 = unknown
  typedef BOOL(WINAPI * GFT)(int*);
  GFT f = (GFT)GetProcAddress(GetModuleHandleA("kernel32"), "GetFirmwareType");
  int t = 0;
  if (f && f(&t)) return t == 1 ? 1 : t == 2 ? 2 : 0;
  SetLastError(0); GetFirmwareEnvironmentVariableA("", "{00000000-0000-0000-0000-000000000000}", NULL, 0);
  DWORD e = GetLastError(); return e == ERROR_INVALID_FUNCTION ? 1 : e == ERROR_NOACCESS ? 2 : 0;
}
static vector<Disk> listDisks() {
  string o = ps("Get-Disk | ForEach-Object { $n=$_.Number; $pc=@(Get-Partition -DiskNumber $n -ErrorAction SilentlyContinue).Count; ($n,$_.FriendlyName,$_.Size,$_.PartitionStyle,$pc,[int]($_.IsBoot -or $_.IsSystem)) -join '|' }");
  vector<Disk> v;
  for (auto& l : split(o, '\n')) { auto f = split(l, '|'); if (f.size() != 6 || f[0].empty() || !isdigit((unsigned char)f[0][0])) continue;
    Disk d; d.n = atoi(f[0].c_str()); d.name = f[1]; d.size = strtoull(f[2].c_str(), 0, 10); d.style = f[3]; d.parts = atoi(f[4].c_str()); d.sys = f[5] == "1"; v.push_back(d); }
  return v;
}
static bool recommended(const Disk& d) { return d.parts == 0 && !d.sys && d.size >= 1073741824ULL; }

// ---------- raw disk helpers ----------
struct Buf { void* p; size_t n; Buf(size_t s) : n(s) { p = VirtualAlloc(NULL, s, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); } ~Buf() { if (p) VirtualFree(p, 0, MEM_RELEASE); } uint8_t* b() { return (uint8_t*)p; } };
static HANDLE openDisk(int n, bool w) { char p[64]; snprintf(p, 64, "\\\\.\\PhysicalDrive%d", n); return CreateFileA(p, GENERIC_READ | (w ? GENERIC_WRITE : 0), FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0); }
static bool rdSec(HANDLE h, uint64_t lba, void* buf, DWORD secs) { LARGE_INTEGER o; o.QuadPart = (LONGLONG)(lba * 512); if (!SetFilePointerEx(h, o, 0, FILE_BEGIN)) return false; DWORD n = 0; return ReadFile(h, buf, secs * 512, &n, 0) && n == secs * 512; }
static bool wrSec(HANDLE h, uint64_t lba, const void* buf, DWORD secs) { LARGE_INTEGER o; o.QuadPart = (LONGLONG)(lba * 512); if (!SetFilePointerEx(h, o, 0, FILE_BEGIN)) return false; DWORD n = 0; return WriteFile(h, buf, secs * 512, &n, 0) && n == secs * 512; }
static int systemDisk() {
  char dr[16] = "C:"; GetEnvironmentVariableA("SystemDrive", dr, 16);
  string v = string("\\\\.\\") + dr;
  HANDLE h = CreateFileA(v.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0); if (h == INVALID_HANDLE_VALUE) return -1;
  STORAGE_DEVICE_NUMBER s; DWORD n; bool ok = DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, 0, 0, &s, sizeof s, &n, 0); CloseHandle(h); return ok ? (int)s.DeviceNumber : -1;
}
struct Check { bool ok; string why; uint64_t firstStart; };
static Check checkBootDisk(int disk) {
  Check c{false, "", 0}; HANDLE h = openDisk(disk, false); if (h == INVALID_HANDLE_VALUE) { c.why = "cannot open the boot disk (run as administrator)"; return c; }
  DISK_GEOMETRY_EX g; DWORD n; if (!DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, 0, 0, &g, sizeof g, &n, 0)) { c.why = "cannot read disk geometry"; CloseHandle(h); return c; }
  if (g.Geometry.BytesPerSector != 512) { c.why = "this disk does not use 512-byte sectors"; CloseHandle(h); return c; }
  vector<char> b(65536); if (!DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, 0, 0, b.data(), (DWORD)b.size(), &n, 0)) { c.why = "cannot read the partition table"; CloseHandle(h); return c; }
  auto* d = (DRIVE_LAYOUT_INFORMATION_EX*)b.data();
  if (d->PartitionStyle != PARTITION_STYLE_MBR) { c.why = "the Windows disk uses GPT; this ClaudeOS build only boots through legacy BIOS (MBR)"; CloseHandle(h); return c; }
  c.firstStart = ~0ULL;
  for (DWORD i = 0; i < d->PartitionCount; i++) { auto& p = d->PartitionEntry[i]; if (p.PartitionLength.QuadPart == 0) continue;
    if (p.Mbr.PartitionType == 0x42) { c.why = "the Windows disk is a dynamic disk, which is not supported"; CloseHandle(h); return c; }
    uint64_t s = (uint64_t)p.StartingOffset.QuadPart / 512; if (s < c.firstStart) c.firstStart = s; }
  Buf mbr(512); if (!rdSec(h, 0, mbr.p, 1) || mbr.b()[510] != 0x55 || mbr.b()[511] != 0xAA) { c.why = "the MBR is unreadable or has no 55AA signature"; CloseHandle(h); return c; }
  CloseHandle(h);
  uint64_t need = (core_img_len + 511) / 512 + 8;
  if (c.firstStart < need + 1) { c.why = "there is not enough free space before the first partition to embed GRUB"; return c; }
  c.ok = true; return c;
}

// ---------- files ----------
static void mkdirs(string p) { for (size_t i = 3; i < p.size(); i++) if (p[i] == '\\') { string s = p.substr(0, i); CreateDirectoryA(s.c_str(), 0); } CreateDirectoryA(p.c_str(), 0); }
static bool writeFile(const string& path, const void* d, size_t n) { FILE* f = fopen(path.c_str(), "wb"); if (!f) return false; bool ok = fwrite(d, 1, n, f) == n; fclose(f); return ok; }

static bool backup(int disk, const string& letterDir, string& where) {
  HANDLE h = openDisk(disk, false); if (h == INVALID_HANDLE_VALUE) return false; Buf b(2048 * 512);
  bool ok = rdSec(h, 0, b.p, 2048); CloseHandle(h); if (!ok) return false;
  char num[16]; snprintf(num, 16, "%d", disk);
  string a = letterDir + "\\backup\\disk" + num + "-first-1MB.bin"; mkdirs(letterDir + "\\backup");
  char up[MAX_PATH] = "C:\\"; GetEnvironmentVariableA("USERPROFILE", up, MAX_PATH);
  string c = string(up) + "\\claudeos-disk" + num + "-backup.bin";
  if (!writeFile(a, b.p, b.n) || !writeFile(c, b.p, b.n)) return false; where = a + "  and  " + c; return true;
}
static bool installGrub(int disk, string& err) {
  HANDLE h = openDisk(disk, true); if (h == INVALID_HANDLE_VALUE) { err = "cannot open the disk for writing"; return false; }
  if (boot_img_len != 512) { err = "boot.img payload has the wrong size"; CloseHandle(h); return false; }
  size_t secs = (core_img_len + 511) / 512; Buf core(secs * 512), mbr(512), chk(secs * 512 > 512 ? secs * 512 : 512);
  memset(core.p, 0, core.n); memcpy(core.p, core_img, core_img_len);
  uint64_t start = 2; uint16_t len = (uint16_t)(secs - 1), seg = 0x820;  // blocklist for the rest of core.img (embedded at LBA 1)
  memcpy(core.b() + 500, &start, 8); memcpy(core.b() + 508, &len, 2); memcpy(core.b() + 510, &seg, 2);
  if (!rdSec(h, 0, mbr.p, 1)) { err = "cannot read the MBR"; CloseHandle(h); return false; }
  Buf boot(512); memcpy(boot.p, boot_img, 512);
  memcpy(boot.b() + 0x1B8, mbr.b() + 0x1B8, 0x48);          // keep disk signature + partition table + 55AA
  uint64_t ks = 1; memcpy(boot.b() + 0x5C, &ks, 8);          // core.img starts at LBA 1
  if (!wrSec(h, 1, core.p, (DWORD)secs)) { err = "writing core.img failed (nothing was written to sector 0)"; CloseHandle(h); return false; }
  if (!rdSec(h, 1, chk.p, (DWORD)secs) || memcmp(chk.p, core.p, core.n)) { err = "core.img verification failed (sector 0 untouched)"; CloseHandle(h); return false; }
  if (!wrSec(h, 0, boot.p, 1)) { err = "writing the boot sector failed; restore with --restore"; CloseHandle(h); return false; }
  Buf v(512); if (!rdSec(h, 0, v.p, 1) || memcmp(v.p, boot.p, 512)) { err = "boot sector verification failed; restore with --restore"; CloseHandle(h); return false; }
  CloseHandle(h); return true;
}

// ---------- elevated stage ----------
static int install(const string& mode, int diskArg) {
  cout << "ClaudeOS installer (administrator stage)\n";
  if (!IsUserAnAdmin()) return fail("administrator permission is required");
  int fw = firmware(); cout << "1/8 Firmware: " << (fw == 1 ? "legacy BIOS" : fw == 2 ? "UEFI" : "unknown") << "\n";
  if (fw != 1) return fail("this PC boots in UEFI mode (or the mode is unknown). This ClaudeOS build needs legacy BIOS / CSM. Enable CSM in firmware setup, or use a USB stick with the ISO.");
  int sd = systemDisk(); if (sd < 0) return fail("could not find the disk that holds Windows");
  cout << "2/8 Windows is on disk " << sd << ". Checking it can carry GRUB...\n";
  Check c = checkBootDisk(sd); if (!c.ok) return fail(c.why);
  cout << "    OK (first partition starts at sector " << c.firstStart << ")\n";
  string letter;
  if (dry) cout << "3/8 [dry run] would " << (mode == "create" ? "shrink the Windows drive by 2.3 GB and create a 2 GB FAT32 drive" : "create a 2 GB FAT32 drive on disk " + to_string(diskArg)) << "\n";
  else {
    cout << "3/8 Preparing a 2 GB ClaudeOS drive...\n"; string s;
    char sdl[8] = "C"; GetEnvironmentVariableA("SystemDrive", sdl, 8); sdl[1] = 0;
    if (mode == "create") s = string("$ErrorActionPreference='Stop';try{$c=Get-Partition -DriveLetter ") + sdl + ";$s=Get-PartitionSupportedSize -DriveLetter " + sdl + ";$new=$c.Size-2300MB;if($new -lt $s.SizeMin){throw 'Not enough shrinkable free space on the Windows drive (about 2.3 GB needed)'};Resize-Partition -DriveLetter " + sdl + " -Size $new;$p=New-Partition -DiskNumber $c.DiskNumber -Size 2GB -AssignDriveLetter;Format-Volume -DriveLetter $p.DriveLetter -FileSystem FAT32 -NewFileSystemLabel CLAUDEOS -Confirm:$false|Out-Null;Write-Output ('LETTER='+$p.DriveLetter)}catch{Write-Output ('FAILED='+$_.Exception.Message)}";
    else { string n = to_string(diskArg); s = "$ErrorActionPreference='Stop';try{$d=Get-Disk -Number " + n + ";if($d.PartitionStyle -eq 'RAW'){Initialize-Disk -Number " + n + " -PartitionStyle MBR};if($d.Size -gt 2GB){$p=New-Partition -DiskNumber " + n + " -Size 2GB -AssignDriveLetter}else{$p=New-Partition -DiskNumber " + n + " -UseMaximumSize -AssignDriveLetter};Format-Volume -DriveLetter $p.DriveLetter -FileSystem FAT32 -NewFileSystemLabel CLAUDEOS -Confirm:$false|Out-Null;Write-Output ('LETTER='+$p.DriveLetter)}catch{Write-Output ('FAILED='+$_.Exception.Message)}"; }
    string o = ps(s); size_t i = o.find("LETTER="); if (i == string::npos) return fail("could not create the drive: " + o);
    letter = string(1, o[i + 7]); Sleep(2500);
  }
  string root = letter.empty() ? "X:" : letter + ":"; string dir = root + "\\claudeos";
  if (dry) cout << "4/8 [dry run] would copy claudeos.elf and grub.cfg to " << dir << "\n";
  else {
    cout << "4/8 Copying ClaudeOS to " << dir << "\n"; mkdirs(dir + "\\boot"); mkdirs(dir + "\\grub");
    if (!writeFile(dir + "\\boot\\claudeos.elf", claudeos_elf, claudeos_elf_len) || !writeFile(dir + "\\grub\\grub.cfg", GRUB_CFG, strlen(GRUB_CFG))) return fail("could not write files to " + dir);
  }
  string where;
  if (dry) cout << "5/8 [dry run] would back up the first 1 MB of disk " << sd << "\n";
  else { cout << "5/8 Backing up the first 1 MB of disk " << sd << "...\n"; if (!backup(sd, dir, where)) return fail("backup failed, GRUB was NOT installed"); cout << "    saved: " << where << "\n"; }
  cout << "\n6/8 About to write GRUB to the start of disk " << sd << " (the disk Windows boots from).\n";
  cout << "    If anything goes wrong: run  boot.exe --restore <backup file> " << sd << "  or use a Windows recovery disk and run  bootrec /fixmbr\n";
  if (dry) { cout << "7/8 [dry run] stopping here. Nothing was changed.\n"; pause_(); return 0; }
  if (ask("    Type INSTALL to continue: ") != "INSTALL") return fail("cancelled, GRUB was not installed (the 2 GB drive and files remain)");
  string err; cout << "7/8 Writing GRUB...\n"; if (!installGrub(sd, err)) return fail(err);
  cout << "8/8 Done. At the next start GRUB shows a menu with Windows and ClaudeOS.\n";
  string r = ask("Restart now? (y/n): "); if (r == "y" || r == "Y") system("shutdown /r /t 5 /c \"Restarting into the ClaudeOS boot menu\"");
  return 0;
}
static int restore(const string& file, int disk) {
  if (!IsUserAnAdmin()) return fail("run as administrator");
  FILE* f = fopen(file.c_str(), "rb"); if (!f) return fail("cannot open " + file);
  Buf b(2048 * 512); size_t n = fread(b.p, 1, b.n, f); fclose(f); if (n != b.n) return fail("backup file has the wrong size");
  if (ask("Restore the first 1 MB of disk " + to_string(disk) + " from this backup? Type RESTORE: ") != "RESTORE") return fail("cancelled");
  HANDLE h = openDisk(disk, true); if (h == INVALID_HANDLE_VALUE || !wrSec(h, 0, b.p, 2048)) return fail("restore failed");
  CloseHandle(h); cout << "Restored.\n"; pause_(); return 0;
}
static int elevate(const string& args) {
  char exe[MAX_PATH]; GetModuleFileNameA(0, exe, MAX_PATH);
  SHELLEXECUTEINFOA s; memset(&s, 0, sizeof s); s.cbSize = sizeof s; s.fMask = SEE_MASK_NOCLOSEPROCESS; s.lpVerb = "runas"; s.lpFile = exe; s.lpParameters = args.c_str(); s.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteExA(&s)) return -1; WaitForSingleObject(s.hProcess, INFINITE); DWORD c = 0; GetExitCodeProcess(s.hProcess, &c); CloseHandle(s.hProcess); return (int)c;
}

int main(int argc, char** argv) {
  string mode; int disk = -1; string rf; int rd = -1;
  for (int i = 1; i < argc; i++) { string a = argv[i];
    if (a == "--dry-run") dry = true; else if (a == "--install") mode = "install"; else if (a == "--mode" && i + 1 < argc) mode = argv[++i];
    else if (a == "--disk" && i + 1 < argc) disk = atoi(argv[++i]); else if (a == "--restore" && i + 2 < argc) { rf = argv[++i]; rd = atoi(argv[++i]); } }
  if (!rf.empty()) return restore(rf, rd);
  if (mode == "create" || mode == "disk") return install(mode, disk);

  cout << "==========================================\n  ClaudeOS setup (boot.exe)\n==========================================\n";
  cout << "Detected: you are running Windows, so this installer can set up ClaudeOS next to it.\n(If you can flash a USB stick with ClaudeOS.iso instead, you do not need this program.)\n\n";
  int fw = firmware();
  if (fw == 2) { cout << "This PC starts in UEFI mode. This ClaudeOS build only boots through legacy BIOS (CSM), so installing here would not work.\nEnable CSM/Legacy boot in the firmware settings, or use a USB stick with the ISO.\n"; pause_(); return 1; }
  if (fw == 0) cout << "Warning: could not tell whether this PC uses legacy BIOS or UEFI. The administrator stage will check again.\n";
  cout << "Reading disks...\n"; auto disks = listDisks();
  cout << "\nWhere do you want to install ClaudeOS?\n\n";
  bool any = false;
  for (auto& d : disks) { bool rec = recommended(d); any |= rec; cout << "  [" << d.n << "] " << d.name << "  " << gb(d.size) << "  " << d.style << (d.parts == 0 ? "  (empty)" : "  (in use)") << (d.sys ? "  (Windows)" : "") << (rec ? "   <-- Recommended" : "   (not selectable)") << "\n"; }
  if (disks.empty()) cout << "  (no disks listed; try running boot.exe as administrator)\n";
  cout << "  [N] Create a new 2 GB Local Drive from free space on the Windows drive\n  [Q] Quit\n\n";
  if (!any) cout << "No empty disk with more than 1 GB was found, so please create a new drive (choose N). This needs administrator permission.\n\n";
  string pick = ask("Your choice: "); if (pick == "Q" || pick == "q" || pick.empty()) return 0;
  string args; string what;
  if (pick == "N" || pick == "n") { args = "--install --mode create"; what = "shrink your Windows drive by about 2.3 GB and create a 2 GB drive named CLAUDEOS"; }
  else { int n = atoi(pick.c_str()); const Disk* sel = nullptr; for (auto& d : disks) if (d.n == n && recommended(d)) sel = &d; if (!sel) return fail("that disk cannot be selected");
    args = "--install --mode disk --disk " + to_string(n); what = "create a 2 GB drive on disk " + to_string(n) + " (" + sel->name + "); the rest of that disk is not touched"; }
  if (dry) args += " --dry-run";
  cout << "\nClaudeOS will " << what << ", copy itself there, and install GRUB so that a menu with Windows and ClaudeOS appears when the PC starts.\n";
  if (ask("Type YES to continue (Windows will ask for administrator permission next): ") != "YES") return 0;
  int rc = elevate(args); if (rc < 0) return fail("administrator permission was not granted");
  return rc;
}

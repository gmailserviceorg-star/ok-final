#!/bin/sh
# Boots ClaudeOS.iso in QEMU with no window, drives it with real keystrokes and checks the serial log.
# Needs: qemu-system-x86 and python3.   Usage: ./test-qemu.sh [ClaudeOS.iso]
ISO=${1:-ClaudeOS.iso}
command -v qemu-system-i386 >/dev/null || { echo "Install QEMU first: sudo apt install qemu-system-x86"; exit 1; }
[ -f "$ISO" ] || { echo "Build the ISO first: ./build.sh"; exit 1; }
rm -f /tmp/cos.serial /tmp/cos.mon /tmp/cos-disk.img; qemu-img create -f raw /tmp/cos-disk.img 64M >/dev/null
qemu-system-i386 -cdrom "$ISO" -m 256 -boot d -display none -serial file:/tmp/cos.serial \
  -monitor unix:/tmp/cos.mon,server,nowait -drive file=/tmp/cos-disk.img,format=raw,if=ide &
QPID=$!
python3 - <<'PY'
import socket,time,sys,os
def wait_for(txt,t=60):
    end=time.time()+t
    while time.time()<end:
        try:
            if txt in open('/tmp/cos.serial',errors='ignore').read():return True
        except FileNotFoundError:pass
        time.sleep(0.5)
    return False
for _ in range(50):
    if os.path.exists('/tmp/cos.mon'):break
    time.sleep(0.2)
m=socket.socket(socket.AF_UNIX);m.connect('/tmp/cos.mon');time.sleep(0.5);m.recv(4096)
def key(k):m.send(('sendkey %s\n'%k).encode());time.sleep(0.12)
KM={' ':'spc','/':'slash','\\':'backslash','*':'shift-8','.':'dot','-':'minus','_':'shift-minus','\n':'ret'}
def typ(s):
    for c in s:
        if c in KM:key(KM[c])
        elif c.isupper():key('shift-'+c.lower())
        else:key(c)
ok=True
def check(name,cond):
    global ok;print(('PASS  ' if cond else 'FAIL  ')+name);ok=ok and cond
check('kernel booted to the setup screen',wait_for('STATE: setup'))
key('shift-f10');time.sleep(1.5)
typ('claude\\run /f *.skip \\else\n');time.sleep(2);key('esc');time.sleep(1)
key('ret');time.sleep(1)
check('setup finished (skip files created, Finish clicked)',wait_for('STATE: login',10))
typ('claude\n');time.sleep(2)
check('login with the default account works',wait_for('STATE: desktop',10))
m.send(b'screendump /tmp/cos-desktop.ppm\n');time.sleep(1.5)
check('screenshot written (/tmp/cos-desktop.ppm)',os.path.exists('/tmp/cos-desktop.ppm') and os.path.getsize('/tmp/cos-desktop.ppm')>100000)
m.send(b'quit\n')
print('\nSerial log:\n'+open('/tmp/cos.serial',errors='ignore').read())
sys.exit(0 if ok else 1)
PY
RC=$?; kill $QPID 2>/dev/null; exit $RC

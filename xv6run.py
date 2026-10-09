#!/usr/bin/env python3
"""Chay xv6 trong QEMU, go lan luot tung lenh, in lai phien lam viec.

Dung:  python3 xv6run.py 'lenh 1' 'lenh 2' ...
Lenh dau tien luon bi console xv6 an mat ky tu dau, nen script tu chen
mot lenh moi vo hai o dau de hung phan mat mat do.
"""
import subprocess, sys, threading, time

QEMU = ["qemu-system-riscv64", "-machine", "virt", "-bios", "none",
        "-kernel", "kernel/kernel", "-m", "128M", "-smp", "3", "-nographic",
        "-global", "virtio-mmio.force-legacy=false",
        "-drive", "file=fs.img,if=none,format=raw,id=x0",
        "-device", "virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0"]

def main(cmds):
    p = subprocess.Popen(QEMU, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True, bufsize=1)
    out = []
    def reader():
        for line in p.stdout:
            out.append(line)
    t = threading.Thread(target=reader, daemon=True)
    t.start()

    time.sleep(3)                       # cho kernel boot xong
    for c in ["echo khoi dong"] + cmds: # lenh moi hung ky tu bi mat
        p.stdin.write(c + "\n")
        p.stdin.flush()
        time.sleep(1.2)                 # cho shell echo + chuong trinh chay
    time.sleep(1)
    p.kill()
    t.join(timeout=2)
    print("".join(out))

if __name__ == "__main__":
    main(sys.argv[1:])

"""Compile and run real firmware processing with a native C compiler (no ESP required)."""
import argparse
from pathlib import Path
import shlex
import subprocess
import tempfile

parser=argparse.ArgumentParser()
parser.add_argument("--cc",default="cc",help='Compiler command, e.g. "gcc" or "zig cc"')
args=parser.parse_args()
root=Path(__file__).resolve().parent.parent
sources=["relogio/battery_estimator.c","sensores/max30102/finger_detect.c",
         "sensores/max30102/heart_rate.c","sensores/max30102/ppg_filter.c",
         "sensores/max30102/spo2.c","sensores/ltr390/ltr390_process.c"]
with tempfile.TemporaryDirectory(prefix="idroid-tests-") as work:
    exe=Path(work)/"processing.exe"
    command=shlex.split(args.cc)+["-std=c11","-Wall","-Wextra","-Werror","-O1",
        "-I"+str(root/"tests/stubs"),"-I"+str(root/"main"),
        "-I"+str(root/"main/relogio"),"-I"+str(root/"components/chsc6x_touch/include"),"-I"+str(root/"main/sensores/max30102"),
        "-I"+str(root/"main/sensores/ltr390"),str(root/"tests/test_processing.c")]
    command += [str(root/"main"/source) for source in sources]+["-lm","-o",str(exe)]
    subprocess.run(command,check=True)
    subprocess.run([str(exe)],check=True)
    driver_sources=["relogio/rtc_pcf8563.c","sensores/max30102/max30102_hw.c"]
    driver_command=shlex.split(args.cc)+["-std=c11","-Wall","-Wextra","-Werror","-Wno-unused-parameter","-O1",
        "-I"+str(root/"tests/stubs"),"-I"+str(root/"main"),"-I"+str(root/"main/relogio"),
        "-I"+str(root/"main/sensores/max30102"),str(root/"tests/test_drivers.c")]
    driver_command += [str(root/"main"/source) for source in driver_sources]+["-o",str(exe)]
    subprocess.run(driver_command,check=True)
    subprocess.run([str(exe)],check=True)

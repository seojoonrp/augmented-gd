# Cuts the sound effects out of resources/sfx/source/ into
# resources/sfx/<name>.wav (16-bit): trim, short fade-in against clicks,
# fade-out, peak level. Decodes with GD's own fmod.dll through ctypes so
# nothing needs installing. Not run by build.ps1 (the .wav files are committed).
#
#   py -3 scripts/sfxcut.py                    write every cut in CUTS
#   py -3 scripts/sfxcut.py --analyze FILE     print FILE's level every 20 ms
#
# FMOD_DLL overrides the dll's path.

import ctypes
import math
import os
import struct
import sys
import wave

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SOURCE = os.path.join(ROOT, 'resources', 'sfx', 'source')
OUT = os.path.join(ROOT, 'resources', 'sfx')
FMOD_DLL = os.environ.get('FMOD_DLL', r'C:\Program Files (x86)\Steam\steamapps\common\Geometry Dash\fmod.dll')

# name -> cut, times in seconds of the source. fade_out = (from, to), the sound
# ends at `to`. starts skip the mp3 encoder delay (found with --analyze).
CUTS = {
    'hover.wav': dict(src='hover.mp3', start=0.019, end=0.048, fade_in=0.002, fade_out=(0.038, 0.048)),
    # has to end inside the draft's 1 s send-off
    'select.wav': dict(src='select.mp3', start=0.021, end=0.56, fade_out=(0.40, 0.56)),
    # blast + a bit of the rumble, the source goes on for 15 s
    'missile.wav': dict(src='missile.mp3', start=0.046, end=1.35, fade_out=(0.45, 1.35)),
}


# ---------------------------------------------------------------- decoding

FMOD_VERSION = 0x00020223          # Geode/fmod/fmod_common.h
FMOD_OUTPUTTYPE_NOSOUND_NRT = 4
FMOD_2D = 0x00000008
FMOD_OPENONLY = 0x00002000
FMOD_ACCURATETIME = 0x00004000
FMOD_TIMEUNIT_PCMBYTES = 0x00000004
FORMATS = {1: ('b', 1), 2: ('h', 2), 5: ('f', 4)}   # PCM8, PCM16, PCMFLOAT


def decode(path):
    """(rate, channels, frames): frames are tuples of floats in -1..1."""
    fmod = ctypes.WinDLL(FMOD_DLL)
    system = ctypes.c_void_p()
    def check(result, what):
        if result != 0:
            raise RuntimeError(f'FMOD {what} failed: {result}')
    check(fmod.FMOD_System_Create(ctypes.byref(system), FMOD_VERSION), 'System_Create')
    check(fmod.FMOD_System_SetOutput(system, FMOD_OUTPUTTYPE_NOSOUND_NRT), 'SetOutput')
    check(fmod.FMOD_System_Init(system, 4, 0, None), 'Init')
    sound = ctypes.c_void_p()
    mode = FMOD_2D | FMOD_OPENONLY | FMOD_ACCURATETIME
    check(fmod.FMOD_System_CreateSound(system, path.encode('utf-8'), mode, None, ctypes.byref(sound)), 'CreateSound')
    stype, fmt, channels, bits = ctypes.c_int(), ctypes.c_int(), ctypes.c_int(), ctypes.c_int()
    check(fmod.FMOD_Sound_GetFormat(sound, ctypes.byref(stype), ctypes.byref(fmt), ctypes.byref(channels), ctypes.byref(bits)), 'GetFormat')
    rate, priority = ctypes.c_float(), ctypes.c_int()
    check(fmod.FMOD_Sound_GetDefaults(sound, ctypes.byref(rate), ctypes.byref(priority)), 'GetDefaults')
    length = ctypes.c_uint()
    check(fmod.FMOD_Sound_GetLength(sound, ctypes.byref(length), FMOD_TIMEUNIT_PCMBYTES), 'GetLength')
    if fmt.value not in FORMATS:
        raise RuntimeError(f'unhandled sample format {fmt.value}')
    code, width = FORMATS[fmt.value]
    buf = ctypes.create_string_buffer(length.value)
    read = ctypes.c_uint()
    fmod.FMOD_Sound_ReadData(sound, buf, length.value, ctypes.byref(read))
    raw = buf.raw[:read.value]
    count = len(raw) // width
    values = struct.unpack('<' + code * count, raw[:count * width])
    scale = {'b': 128.0, 'h': 32768.0, 'f': 1.0}[code]
    n = channels.value
    frames = [tuple(values[i + c] / scale for c in range(n)) for i in range(0, count - count % n, n)]
    fmod.FMOD_Sound_Release(sound)
    fmod.FMOD_System_Release(system)
    return int(rate.value), n, frames


# ---------------------------------------------------------------- cutting

def cut(src, start, end, fade_in=0.004, fade_out=None, peak=0.9):
    rate, channels, frames = decode(os.path.join(SOURCE, src))
    a, b = int(start * rate), min(len(frames), int(end * rate))
    frames = frames[a:b]
    out = []
    for i, f in enumerate(frames):
        t = start + i / rate
        g = min(1.0, (i / rate) / fade_in) if fade_in > 0 else 1.0
        if fade_out:
            f0, f1 = fade_out
            if t >= f1:
                g = 0.0
            elif t > f0:
                u = (t - f0) / (f1 - f0)
                g *= (1.0 - u) ** 2          # eases out: most of the drop early
        out.append(tuple(v * g for v in f))
    top = max(1e-9, max(abs(v) for f in out for v in f))
    return rate, channels, [tuple(v * peak / top for v in f) for f in out]


def write(name, rate, channels, frames):
    path = os.path.join(OUT, name)
    with wave.open(path, 'wb') as w:
        w.setnchannels(channels)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b''.join(struct.pack('<' + 'h' * channels, *(int(max(-1.0, min(1.0, v)) * 32767) for v in f)) for f in frames))
    print(f'{name}: {len(frames) / rate:.2f} s, {rate} Hz, {channels} ch, {os.path.getsize(path) // 1024} KB')


def analyze(path):
    rate, channels, frames = decode(path)
    print(f'{os.path.basename(path)}: {len(frames) / rate:.2f} s, {rate} Hz, {channels} ch')
    win = max(1, int(0.02 * rate))
    for k in range(0, len(frames), win):
        chunk = [sum(f) / channels for f in frames[k:k + win]]
        rms = math.sqrt(sum(v * v for v in chunk) / len(chunk))
        peak = max(abs(v) for v in chunk)
        zc = sum(1 for p, q in zip(chunk, chunk[1:]) if (p < 0) != (q < 0)) / (len(chunk) / rate) / 2
        bar = '#' * int(rms * 60)
        print(f'  {k / rate:6.2f}s rms {rms:.3f} peak {peak:.2f} ~{zc:5.0f}Hz {bar}')


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == '--analyze':
        analyze(sys.argv[2])
        return
    for name, spec in CUTS.items():
        write(name, *cut(**spec))


if __name__ == '__main__':
    main()

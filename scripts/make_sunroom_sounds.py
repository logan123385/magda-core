#!/usr/bin/env python3
"""Synthesize SUNROOM's original, reproducible CC0 starter library. No samples used."""
from pathlib import Path
import json
import wave
import numpy as np

RATE = 48000
ROOT = Path(__file__).resolve().parents[1] / 'resources/sunroom/Sounds'
ROOT.mkdir(parents=True, exist_ok=True)
rng = np.random.default_rng(73084)
manifest = []

def save(name, audio, kind):
    audio = np.asarray(audio, dtype=np.float64)
    if audio.ndim == 1:
        audio = np.stack([audio, np.roll(audio, int(RATE * .003))], axis=1)
    audio -= audio.mean(axis=0)
    fade = min(int(RATE*.04), len(audio)//8)
    audio[:min(96,fade)] *= np.linspace(0,1,min(96,fade))[:,None]
    audio[-fade:] *= np.linspace(1,0,fade)[:,None]
    peak = np.max(np.abs(audio))
    if peak > 0: audio *= .68 / peak
    assert np.isfinite(audio).all()
    pcm = (audio * 32767).astype('<i2')
    with wave.open(str(ROOT/(name+'.wav')), 'wb') as f:
        f.setparams((2,2,RATE,len(audio),'NONE','not compressed')); f.writeframes(pcm.tobytes())
    manifest.append({'file':name+'.wav','kind':kind,'seconds':round(len(audio)/RATE,3),
                     'peak_dbfs':round(float(20*np.log10(np.max(np.abs(audio)))),2)})

def noise(n, low=30, high=8000):
    freq=np.fft.rfftfreq(n,1/RATE)
    shape=(1-np.exp(-(freq/max(1,low))**4))*np.exp(-(freq/high)**4)
    raw=np.fft.irfft(np.fft.rfft(rng.normal(size=n))*shape,n)
    return raw/max(np.std(raw),1e-8)

for i in range(8):
    t=np.arange(int(RATE*(1.2+i*.06)))/RATE
    phase=2*np.pi*((43+i*2)*t+(130+i*5)*.032*(1-np.exp(-t/.032)))
    kick=np.sin(phase)*np.exp(-t/(.19+i*.025))
    kick+=noise(len(t),1200,5000)*np.exp(-t/.006)*.045
    save(f'Heartbeat {i+1:02d}',np.stack([kick,kick],axis=1),'kick')
for i in range(8):
    t=np.arange(int(RATE*.9))/RATE
    body=np.sin(2*np.pi*(125+i*11)*t+1.5*(1-np.exp(-t/.03)))*np.exp(-t/.13)
    body+=.4*np.sin(2*np.pi*(214+i*17)*t)*np.exp(-t/.067)
    body+=noise(len(t),800,6000)*np.exp(-t/.027)*.3
    save(f'Earth hand {i+1:02d}',body,'hand percussion')
for i in range(8):
    t=np.arange(int(RATE*(.3+i*.035)))/RATE
    env=np.exp(-t/(.018+i*.01))
    save(f'Sun dust {i+1:02d}',noise(len(t),4500,13000)*env,'shaker/hat')
for i in range(8):
    t=np.arange(RATE*7)/RATE; freq=146.8324*2**(i/12)
    sig=sum(np.sin(2*np.pi*freq*ratio*t)*np.exp(-t/(1.9/(j+1)))/(j+1)
            for j,ratio in enumerate([1,2,2.756,4.071]))
    echoed=sig.copy()
    for delay,level in [(int(RATE*.536),.42),(int(RATE*1.072),.23)]:
        echoed[delay:]+=sig[:-delay]*level
    save(f'Glass mote {i+1:02d}',echoed,'pitched bell: D4 upward in semitones')
for i in range(8):
    t=np.arange(RATE*12)/RATE
    air=noise(len(t),400+i*130,2200+i*380)
    env=np.sin(np.pi*t/12)**2*(.65+.2*np.sin(2*np.pi*t*(.13+i*.01)))
    shimmering=air*env+.07*np.sin(2*np.pi*(174.614+i*23.17)*t)*env
    left=shimmering;right=np.roll(shimmering,int(RATE*(.021+i*.003)))
    save(f'Horizon air {i+1:02d}',np.stack([left,right],axis=1),'ambient texture')
for i in range(8):
    t=np.arange(RATE*6)/RATE
    env=np.sin(np.pi*t/6)**2
    sweep=np.sin(2*np.pi*(60*t+(250+i*35)*t*t))*env
    mist=noise(len(t),500,6000)*env*(t/6)**2
    save(f'Orbit transition {i+1:02d}',.25*sweep+mist,'transition effect')
(ROOT/'manifest.json').write_text(json.dumps({'sample_rate':RATE,'format':'16-bit stereo PCM WAV',
    'license':'CC0-1.0','source':'Original procedural synthesis; no third-party source audio.',
    'sounds':manifest},indent=2))
(ROOT/'LICENSE.txt').write_text('SUNROOM Original Sound Library\n\nThese 48 original, procedurally synthesized sound recordings are dedicated to the public domain under CC0 1.0 Universal. You may use, edit, and redistribute them, including in commercial music, without attribution. No third-party audio recordings are included.\n\nCC0 legal text: https://creativecommons.org/publicdomain/zero/1.0/legalcode\nGenerator: scripts/make_sunroom_sounds.py\nThe application source remains GPLv3; this dedication applies only to these original WAV recordings.\n')
print(f'Generated {len(manifest)} original stereo WAVs; all finite, faded and below -3 dBFS.')

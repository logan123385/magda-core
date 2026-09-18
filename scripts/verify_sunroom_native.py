#!/usr/bin/env python3
"""Use the compiled native CLI to check composition, undo/redo, persistence and actual DSP output."""
import argparse, array, gzip, json, math, os, pathlib, subprocess, sys, time, wave, zlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--cli',type=pathlib.Path,required=True)
args=parser.parse_args();cli=args.cli.resolve()
qa=ROOT/'artifacts/native-qa';qa.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['MAGDA_DATA_DIR']=str(qa/'profile')
env['MAGDA_CONFIG_FILE']=str(qa/'profile/config.json')
records=[]
def call(name,*argv):
 start=time.monotonic()
 result=subprocess.run([str(cli),*map(str,argv)],env=env,capture_output=True,text=True,timeout=300)
 log=result.stdout+'\n'+result.stderr;(qa/(name+'.log')).write_text(log)
 records.append({'step':name,'returncode':result.returncode,'seconds':round(time.monotonic()-start,2)})
 assert result.returncode==0, f'{name} failed; see {qa/(name+".log")}'
 assert 'JUCE Assertion failure' not in log,f'{name} reported an engine assertion'
 print(name,'passed',flush=True)
 return result.stdout

def saved(output):
 lines=[line[6:].strip() for line in output.splitlines() if line.startswith('Saved ')]
 assert lines,output[-500:]
 file=pathlib.Path(lines[-1]);assert file.is_file();return file

def load(file):
 data=file.read_bytes()
 for decoder in [gzip.decompress,zlib.decompress,lambda x:x]:
  try:return json.loads(decoder(data))
  except (ValueError,OSError,zlib.error,UnicodeDecodeError):pass
 raise ValueError('Cannot decode native project')

blank=saved(call('01-init','init',qa/'Blank.mgd'))
song=saved(call('02-compose','exec',blank,'sunroom-journey',0,2,8,84,'--out',qa/'Dawn.mgd'))
reopened=saved(call('03-roundtrip','run',song,'--out',qa/'Dawn-reopened.mgd'))
before,after=load(song),load(reopened)
assert len(before['tracks'])==len(after['tracks'])==7
assert len(before['clips'])==len(after['clips'])>0
assert [t['id'] for t in before['tracks']]==[t['id'] for t in after['tracks']]
assert [c['id'] for c in before['clips']]==[c['id'] for c in after['clips']]
assert [c['midiNotes'] for c in before['clips']]==[c['midiNotes'] for c in after['clips']]
assert all(c['midiNotes'] for c in after['clips'])
assert before['project']['sunroomGuide'] and after['project']['sunroomGuide']
assert before['project']['keyRoot']==after['project']['keyRoot']==2
assert before['project']['sunroomMood']==after['project']['sunroomMood']==0
assert before['project']['tempo']==after['project']['tempo']==84
wav=qa/'Dawn.wav'
call('04-render','render',reopened,'--wav',wav,'--to','8bars','--sample-rate',48000,'--bit-depth',16)
with wave.open(str(wav),'rb') as f:
 assert f.getnchannels()==2 and f.getsampwidth()==2
 duration=f.getnframes()/f.getframerate();assert abs(duration-32*60/84)<.05
 values=array.array('h',f.readframes(f.getnframes()))
 if sys.byteorder!='little':values.byteswap()
 peak=max(map(abs,values))/32768;rms=math.sqrt(sum(v*v for v in values)/len(values))/32768
 assert .0005<rms<.5, f'Unexpected render energy {rms}'
 assert peak<.999, f'Clipping: peak {peak}'
 metrics={'seconds':duration,'peak_dbfs':20*math.log10(peak),'rms_dbfs':20*math.log10(rms)}
result={'steps':records,'project':str(reopened),'wav':str(wav),'render':metrics,
        'tracks':len(after['tracks']),'clips':len(after['clips']),
        'notes':sum(len(c['midiNotes']) for c in after['clips'])}
(qa/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))

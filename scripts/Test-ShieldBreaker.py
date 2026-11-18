"""Run the actual Java Shield Breaker and Triggerbot together with inert game doubles."""
from pathlib import Path
import argparse,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-only',action='store_true');args=p.parse_args()
java,javac=shutil.which('java'),shutil.which('javac')
if not java or not javac:p.error('JDK 17+ required')
with tempfile.TemporaryDirectory(prefix='onestep-shield-breaker-') as tmp:
    classes=Path(tmp)/'classes'
    subprocess.run([javac,'--release','17','-encoding','UTF-8','-g:none','-d',str(classes),str(ROOT/'src/bridge/WorldBridge.java'),str(ROOT/'src/bridge/AnchorEvents.java'),str(ROOT/'tests/shield_breaker/ShieldBreakerBridgeTest.java'),str(ROOT/'tests/crystal_aura/Event.java'),str(ROOT/'tests/crystal_aura/ClientTickEvents.java')],check=True)
    for mode in (['source'] if args.source_only else ['source','embedded']):
        cp=str(classes) if mode=='source' else os.pathsep.join([str(ROOT/'assets/hybrid'),str(classes)])
        print('Shield Breaker:',mode,flush=True)
        subprocess.run([java,'-ea','-Xcheck:jni','-cp',cp,'net.minecraft.ShieldBreakerBridgeTest'],check=True,cwd=ROOT)

"""Inspect the supplied game shader/layer metadata, without Unity native calls.

Usage: python shader_asset_tests.py GAME_ASSETS UNITYPY_PACKAGE_DIR OUTPUT_JSON
This is a build-time evidence check, not a runtime trainer dependency.
"""
import hashlib
import json
import sys
from pathlib import Path

assets, packages, output = map(Path, sys.argv[1:])
sys.path.insert(0, str(packages.resolve()))
import UnityPy

source = assets / 'sharedassets2.assets'
shaders = [o.read_typetree()['m_ParsedForm']
           for o in UnityPy.load(str(source)).objects if o.type.name == 'Shader']
standard = next(s for s in shaders if s['m_Name'] == 'Standard')
keys = standard['m_KeywordNames']
forward = next(p for p in standard['m_SubShaders'][0]['m_Passes']
               if p['m_State']['m_Name'] == 'FORWARD')
summary = {}
for stage in ['progVertex', 'progFragment']:
    variants = [[keys[i] for i in s['m_KeywordIndices']]
                for s in forward[stage]['m_SubPrograms']]
    alpha = [s for s in variants if '_ALPHABLEND_ON' in s]
    instanced = [s for s in alpha if 'INSTANCING_ON' in s]
    assert alpha and not instanced, (stage, len(alpha), len(instanced))
    summary[stage] = {'alpha_variants': len(alpha),
                      'alpha_instanced_variants': len(instanced)}

tag_source = assets / 'globalgamemanagers'
tags = next(o.read_typetree() for o in UnityPy.load(str(tag_source)).objects
            if o.type.name == 'TagManager')
layers = tags['layers']
required = {'Floor': 9, 'Walls': 10, 'Radar': 12, 'Enemy2': 13,
            'Props': 15, 'Clock': 18, 'Writing': 20, 'PhysicsProp': 21,
            'SmallProp': 22, 'RadarSphere': 24, 'Cieling': 26}
for name, index in required.items():
    assert layers[index] == name, (name, index, layers[index])
for name in ['Wall', 'Prop', 'PhysicsProp', 'Door']:
    assert name in tags['tags'], name

result = {'shader_asset': source.name,
          'sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
          'shader': 'Standard', 'forward': summary, 'layers': required,
          'tag_asset_sha256': hashlib.sha256(tag_source.read_bytes()).hexdigest(),
          'scope': 'Supplied asset metadata; no live GPU/rendering test.'}
output.write_text(json.dumps(result, indent=2) + '\n')
print(f"Supplied Standard shader: {summary['progVertex']['alpha_variants']} vertex / "
      f"{summary['progFragment']['alpha_variants']} fragment alpha variants, "
      'zero alpha+instancing variants; actual wall/floor/prop/radar layers: PASS')

#!/usr/bin/env python3
# Regenerate the dashboard.js per-ECU data arrays (ECU_CONFIG_PARAMS,
# ECU_MEAS_PARAMS, ACTUATOR_TESTS) from the C++ authority headers:
#   include/psa/ecu_params.hpp   all ECUs: config/meas/actuator
#   include/psa/ecu_zones.hpp    BSI telecoding zones -> BMF config
# The dashboard.js data block is auto-generated and MUST NOT be hand-edited.
#
# Usage:  python3 scripts/gen_ecu_data.py            print the JS block to stdout
#         python3 scripts/gen_ecu_data.py --write    splice into dashboard.js
# Then:   python3 scripts/generate_assets.py         re-embed for firmware
import os, re, sys

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(BASE, "include", "psa", "ecu_params.hpp")
ZONES = os.path.join(BASE, "include", "psa", "ecu_zones.hpp")
DASH = os.path.join(BASE, "dashboard", "dashboard.js")

with open(SRC) as f:
    text = f.read()

# 1. enum string arrays:  kXxx[] = { "a","b",...,nullptr };
enum_map = {}
for m in re.finditer(r'inline constexpr const char\* (\w+)\[\] = \{(.*?)\};', text, re.DOTALL):
    name = m.group(1)
    body = m.group(2)
    enum_map[name] = re.findall(r'"([^"]*)"', body)  # stop before 'nullptr'

def hex4(x):
    try:
        return "0x%X" % int(x, 16)
    except Exception:
        return x

def resolve_enum(tok):
    if tok in ("nullptr", "null", "NULL"):
        return None
    return enum_map.get(tok)

# 2. BsiZoneParam config arrays
config_arrays = {}
for m in re.finditer(r'inline constexpr BsiZoneParam (\w+ConfigParams)\[\] = \{(.*?)\};', text, re.DOTALL):
    arr_name, body = m.group(1), m.group(2)
    params = []
    for r in re.findall(r'\{([^}]*)\}', body):
        p = [x.strip() for x in r.split(',')]
        params.append({
            "name": p[3].strip('"'), "zone": hex4(p[0]), "byte": p[1], "mask": p[2],
            "category": p[4].strip('"'), "type": p[5], "enumVals": resolve_enum(p[6] if len(p) > 6 else "nullptr"),
        })
    config_arrays[arr_name] = params

# 2b. BMF config = BSI telecoding zones from ecu_zones.hpp.
#     Its enums are file-scoped (kYesNo differs from ecu_params.hpp), so parse
#     a separate enum map and collect every BsiZoneParam row in file order.
with open(ZONES) as f:
    ztext = f.read()
zenum_map = {}
for m in re.finditer(r'inline constexpr const char\* (\w+)\[\] = \{(.*?)\};', ztext, re.DOTALL):
    zenum_map[m.group(1)] = re.findall(r'"([^"]*)"', m.group(2))
bmf_config = []
for m in re.finditer(r'inline constexpr BsiZoneParam (\w+)\[\] = \{(.*?)\};', ztext, re.DOTALL):
    for r in re.findall(r'\{([^}]*)\}', m.group(2)):
        p = [x.strip() for x in r.split(',')]
        enum_tok = p[6] if len(p) > 6 else "nullptr"
        bmf_config.append({
            "name": p[3].strip('"'), "zone": hex4(p[0]), "byte": p[1], "mask": p[2],
            "category": p[4].strip('"'), "type": p[5],
            "enumVals": zenum_map.get(enum_tok) if enum_tok not in ("nullptr", "null", "NULL") else None,
        })

# 3. LiveDataParam meas arrays
meas_arrays = {}
for m in re.finditer(r'inline constexpr LiveDataParam (\w+MeasParams)\[\] = \{(.*?)\};', text, re.DOTALL):
    arr_name, body = m.group(1), m.group(2)
    params = []
    for r in re.findall(r'\{([^}]*)\}', body):
        p = [x.strip() for x in r.split(',')]
        params.append({"name": p[1].strip('"'), "unit": p[2].strip('"') if len(p) > 2 else "", "did": hex4(p[0])})
    meas_arrays[arr_name] = params

# 4. ActuatorTestEntry arrays
act_arrays = {}
for m in re.finditer(r'inline constexpr ActuatorTestEntry (\w+ActuatorTests)\[\] = \{(.*?)\};', text, re.DOTALL):
    arr_name, body = m.group(1), m.group(2)
    tests = []
    for r in re.findall(r'\{([^}]*)\}', body):
        p = [x.strip() for x in r.split(',')]
        tests.append({"id": re.sub(r'^0x', '', p[0], flags=re.I), "name": p[1].strip('"'), "desc": p[2].strip('"') if len(p) > 2 else ""})
    act_arrays[arr_name] = tests

# 5. kEcuParamSets mapping: family -> (configArr, measArr, actArr)
pset = re.search(r'inline constexpr EcuParamSet kEcuParamSets\[\] = \{(.*?)\n\};', text, re.DOTALL).group(1)
families = {}
for row in re.findall(r'\{([^}]*)\}', pset):
    p = [x.strip() for x in row.split(',')]
    families[p[0].strip('"')] = (p[1], p[3] if len(p) > 3 else "nullptr", p[5] if len(p) > 5 else "nullptr")

def js_str(s):
    return "'" + s.replace("'", "\\'") + "'"

L = []
L.append("/* Auto-generated from include/psa/ecu_params.hpp + ecu_zones.hpp — do not edit by hand. */")
L.append("var ECU_CONFIG_PARAMS = { default:{ label:'Configuration', params:[] } };")
L.append("var ECU_MEAS_PARAMS   = { default:{ label:'Measurements', params:[] } };")
L.append("var ACTUATOR_TESTS    = {};")
for fam, (cfg, meas, act) in families.items():
    L.append("")
    L.append("ECU_CONFIG_PARAMS[%s] = {" % js_str(fam))
    L.append("  label:%s," % js_str(fam)); L.append("  params:[")
    cfg_list = bmf_config if fam == "BMF" else (config_arrays.get(cfg, []) if cfg not in ("nullptr","null","NULL") else [])
    for p in cfg_list:
        ev = p["enumVals"]
        ev_js = "null" if ev is None else "[" + ",".join(js_str(v) for v in ev) + "]"
        L.append("    {name:%s, zone:%s, byte:%s, mask:%s, category:%s, type:%s, enumVals:%s},"
                 % (js_str(p["name"]), p["zone"], p["byte"], p["mask"], js_str(p["category"]), js_str(p["type"]), ev_js))
    L.append("  ]"); L.append("};")
    L.append("ECU_MEAS_PARAMS[%s] = {" % js_str(fam))
    L.append("  label:%s," % js_str(fam)); L.append("  params:[")
    for p in meas_arrays.get(meas, []) if meas not in ("nullptr","null","NULL") else []:
        L.append("    {name:%s, unit:%s, did:%s}," % (js_str(p["name"]), js_str(p["unit"]), p["did"]))
    L.append("  ]"); L.append("};")
    L.append("ACTUATOR_TESTS[%s] = [" % js_str(fam))
    for t in act_arrays.get(act, []) if act not in ("nullptr","null","NULL") else []:
        L.append("  {id:%s, name:%s, desc:%s}," % (js_str(t["id"]), js_str(t["name"]), js_str(t["desc"])))
    L.append("];")

block = "\n".join(L)

if "--write" in sys.argv:
    # Replace the generated region, preserving the hand-written app tail that
    # begins at the "Open Lexia 3 app logic" comment.
    with open(DASH) as f:
        js = f.read()
    marker = "/* ---- Open Lexia 3 app logic"
    idx = js.index(marker)
    with open(DASH, "w") as f:
        f.write(block + "\n\n" + js[idx:])
    sys.stderr.write("wrote generated block into %s\n" % DASH)
else:
    print(block)

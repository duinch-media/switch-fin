#!/usr/bin/env python3
import ftplib
import os
import sys
import json
import io

FTP_HOST = os.environ.get('SWITCH_IP', '.'.join(['192', '168', '1', '17']))
FTP_PORT = int(os.environ.get('SWITCH_PORT', '5000'))
ARR_HOST = os.environ.get('ARR_HOST', 'http://' + '.'.join(['192', '168', '1', '37']))
NRO_PATH = os.environ.get('NRO_PATH', '/tmp/switchfin_artifact/Switchfin-Switch-13/Switchfin.nro')

config_only = '--config-only' in sys.argv

if not config_only and not os.path.exists(NRO_PATH):
    print(f"Error: NRO file not found at {NRO_PATH}")
    sys.exit(1)

print(f"Connecting to Nintendo Switch FTP ({FTP_HOST}:{FTP_PORT})...")
ftp = ftplib.FTP()
try:
    ftp.connect(FTP_HOST, FTP_PORT, timeout=15)
    ftp.login()
    print("FTP login successful.")

    # Upload Switchfin.nro unless --config-only is set
    if not config_only:
        ftp.cwd('/switch')
        file_size = os.path.getsize(NRO_PATH)
        print(f"Uploading Switchfin.nro ({file_size / (1024*1024):.2f} MB) to /switch/Switchfin.nro...")
        with open(NRO_PATH, 'rb') as f:
            ftp.storbinary('STOR /switch/Switchfin.nro', f)
        print("✓ Uploaded /switch/Switchfin.nro")

    # Upload local arr_config.json if present
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    local_cfg = os.path.join(repo_root, 'arr_config.json')
    ftp.cwd('/switch/Switchfin')

    if os.path.exists(local_cfg):
        print(f"Uploading local arr_config.json to Switch...")
        with open(local_cfg, 'rb') as f:
            ftp.storbinary('STOR /switch/Switchfin/arr_config.json', f)
        print("✓ Uploaded /switch/Switchfin/arr_config.json from local file")
    else:
        existing = ftp.nlst()
        if not any('arr_config.json' in x for x in existing):
            default_config = {
                "radarr": {
                    "url": f"{ARR_HOST}:7878",
                    "apiKey": "",
                    "rootFolder": "",
                    "qualityProfileId": 0
                },
                "sonarr": {
                    "url": f"{ARR_HOST}:8989",
                    "apiKey": "",
                    "rootFolder": "",
                    "qualityProfileId": 0
                }
            }
            cfg_bytes = json.dumps(default_config, indent=2).encode('utf-8')
            ftp.storbinary('STOR /switch/Switchfin/arr_config.json', io.BytesIO(cfg_bytes))
            print("✓ Created default /switch/Switchfin/arr_config.json")

    # Upload local health.json (app-health endpoint + key; git-ignored, never committed)
    local_health = os.path.join(repo_root, 'health.json')
    if os.path.exists(local_health):
        with open(local_health, 'rb') as f:
            ftp.storbinary('STOR /switch/Switchfin/health.json', f)
        print("✓ Uploaded /switch/Switchfin/health.json from local file")
    else:
        print("ℹ No local health.json: telemetry will only be queued on the Switch (see resources/health.example.json)")

    ftp.quit()
    print("\n🎉 Deployment completed successfully!")
except Exception as e:
    print(f"\n❌ FTP Error: {e}")
    sys.exit(1)

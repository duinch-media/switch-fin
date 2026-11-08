#!/usr/bin/env python3
import ftplib
import os
import sys
import json
import io

FTP_HOST = os.environ.get('SWITCH_IP', '127.0.0.1')
FTP_PORT = int(os.environ.get('SWITCH_PORT', '5000'))
ARR_HOST = os.environ.get('ARR_HOST', 'http://127.0.0.1')
NRO_PATH = os.environ.get('NRO_PATH', '/tmp/switchfin_artifact/Switchfin-Switch-13/Switchfin.nro')

if not os.path.exists(NRO_PATH):
    print(f"Error: NRO file not found at {NRO_PATH}")
    sys.exit(1)

print(f"Connecting to Nintendo Switch FTP ({FTP_HOST}:{FTP_PORT})...")
ftp = ftplib.FTP()
try:
    ftp.connect(FTP_HOST, FTP_PORT, timeout=15)
    ftp.login()
    print("FTP login successful.")

    # Ensure /switch directory
    ftp.cwd('/switch')

    # Upload Switchfin.nro directly to /switch/Switchfin.nro (standard single location)
    file_size = os.path.getsize(NRO_PATH)
    print(f"Uploading Switchfin.nro ({file_size / (1024*1024):.2f} MB) to /switch/Switchfin.nro...")
    with open(NRO_PATH, 'rb') as f:
        ftp.storbinary('STOR /switch/Switchfin.nro', f)
    print("✓ Uploaded /switch/Switchfin.nro")

    # Check and upload arr_config.json
    try:
        ftp.cwd('/switch/Switchfin')
        existing = ftp.nlst()
        if not any('arr_config.json' in x for x in existing):
            print("Creating default /switch/Switchfin/arr_config.json...")
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
            print("✓ Created /switch/Switchfin/arr_config.json with default 192.168.1.37 host")
        else:
            print("✓ /switch/Switchfin/arr_config.json already exists (preserved).")
    except Exception as e:
        print(f"Note (arr_config.json): {e}")

    ftp.quit()
    print("\n🎉 Deployment completed successfully!")
except Exception as e:
    print(f"\n❌ FTP Error: {e}")
    sys.exit(1)

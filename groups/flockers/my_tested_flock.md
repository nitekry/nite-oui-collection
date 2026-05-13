# OUI / MAC Prefix Notes

This file consolidates the current tracked prefixes, confidence notes, and weak or removed candidates.

## Original GainSec List

| Prefix | Confidence / Notes | Status |
|---|---|---|
| `70:c9:4e` |  | Active |
| `3c:91:80` |  | Active |
| `d8:f3:bc` |  | Active |
| `80:30:49` |  | Active |
| `14:5a:fc` |  | Active |
| `74:4c:a1` |  | Active |
| `08:3a:88` | BLE Ring conflict | Active |
| `9c:2f:9d` | High confidence | Active |
| `94:08:53` |  | Active |
| `e4:aa:ea` |  | Active |
| `f4:6a:dd` | High confidence | Active |
| `f8:a2:d6` | Low confidence; hit on Sony Media Player | Removed |
| `e0:0a:f6` |  | Active |
| `00:f4:8d` |  | Active |
| `d0:39:57` | High confidence | Active |
| `e8:d0:fc` |  | Active |
| `00:0c:e7` | Possible false positive | Removed |
| `58:8e:81` |  | Active |
| `cc:cc:cc` | No clue; no hits | Removed |

## My Findings

| Prefix | Confidence / Notes |
|---|---|
| `ec:1b:bd` | Solid hits; still testing |
| `3c:71:bf` | Solid hits; still testing |
| `90:35:ea` | High confidence |
| `24:b2:b9` | High confidence |
| `c0:35:32` | High confidence |
| `b8:35:32` | High confidence |
| `e0:4f:43` | Solid hits; still testing |

## New Findings — April 2026

| Prefix | Confidence / Notes |
|---|---|
| `14:b5:cd` | New finding |
| `6c:cd:d6` | New finding |
| `94:2a:6f` | New finding |
| `b8:1e:a4` | High confidence |
| `f4:e2:c6` | New finding |

## Lite-On

| Prefix | Confidence / Notes |
|---|---|
| `58:00:e3` | Possible |
| `70:08:94` | Possible |
| `5c:93:a2` | Still verifying |
| `64:6e:69` | Still verifying |

## Crowdsource

| Prefix | Vendor / Role | Confidence | Source |
|---|---|---|---|
| `00:0c:e7` | MediaTek / possible Flock-adjacent observation | Weak / disputed | Field and pcap discussion |
| `08:3a:88` | Espressif Flock Falcon V2 Wi-Fi module | High | Previous analysis |
| `48:27:ea` | Espressif plausible Flock variant | Low | WiGLE crowdsource |
| `a4:cf:12` | Espressif plausible Flock variant | Low | WiGLE crowdsource |

## Notes

- `00:0c:e7` remains tracked for reference, but it is currently weak and may be a false positive.
- The locally administered entries above should not be treated as stable vendor OUIs.
- `b8:1e:a4` has been promoted into the April 2026 section at high confidence per the current list.

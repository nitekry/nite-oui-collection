# OUI / MAC Prefix Notes

## Original GainSec List

| Prefix | Confidence / Notes |
|---|---|
| `70:c9:4e` |  |
| `3c:91:80` |  |
| `d8:f3:bc` |  |
| `80:30:49` |  |
| `14:5a:fc` |  |
| `74:4c:a1` |  |
| `08:3a:88` | High confidence |
| `9c:2f:9d` |  |
| `94:08:53` |  |
| `e4:aa:ea` |  |
| `f4:6a:dd` | High confidence |
| `f8:a2:d6` | Low confidence - hit on Sony Media Player |
| `e0:0a:f6` |  |
| `00:f4:8d` |  |
| `d0:39:57` | High confidence |
| `e8:d0:fc` |  |
| `00:0c:e7` | Possible false positive |
| `58:8e:81` |  |
| `cc:cc:cc` | No clue - No hits |

## My Findings

| Prefix | Confidence / Notes |
|---|---|
| `ec:1b:bd` | Solid hits still testing |
| `3c:71:bf` | Solid hits still testing |
| `90:35:ea` | High confidence |
| `24:b2:b9` | High confidence |
| `c0:35:32` | High confidence |
| `b8:35:32` | High confidence |
| `e0:4f:43` | Solid hits still testing |


## Lite-On

| Prefix | Confidence / Notes |
|---|---|
| `58:00:e3` | Possible |
| `5c:93:a2` | Still verifying |
| `64:6e:69` | Still verifying |

## Crowdsource

| OUI | Vendor / Role | Confidence | Source |
|---|---|---|---|
| `00:0C:E7` | Flock Safety device (STA/client) | High pcap direct observation 6 devices | This capture |
| `08:3A:88` | Espressif Flock Falcon V2 WiFi module | High prior field capture | Previous analysis |
| `48:27:EA` | Espressif plausible Flock variant | Low | WiGLE crowdsource |
| `A4:CF:12` | Espressif plausible Flock variant | Low |  WiGLE crowdsource |

## Locally Administered MACs Seen Repeatedly

These appear to be locally administered MAC addresses, but they have been observed in several scans miles apart.

| Prefix | Notes |
|---|---|
| `12:ea:14` | Repeated observation |
| `1a:ea:14` | Repeated observation |
| `16:ea:14` | Repeated observation |

## Low Confidence

| Prefix | Notes |
|---|---|
| `92:80:d4` | Low confidence |
| `0e:fe:7b` | Low confidence |
| `16:e6:39` | Low confidence |

## Weak Globals

| Prefix | Notes |
|---|---|
| `00:0c:e7` | MediaTek; observed as a possible false positive |

## Observe repeating locally assigned

```text
12:ea:14
16:ea:14
1a:ea:14
b2:19:21

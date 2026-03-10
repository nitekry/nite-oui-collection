# Wireshark Display Filters

- global vs locally administered MAC addresses
- probe / beacon / action frame hunting
- OUI-based clustering
- behavioral filtering for suspected target devices

This is intended for **802.11 Wi-Fi captures** using fields like `wlan.addr`, `wlan.sa`, and `wlan.da`.

---

## Quick Notes

- `wlan.addr` matches an address appearing anywhere relevant in the frame.
- `wlan.sa` is the **source address**.
- `wlan.da` is the **destination address**.

---

# Basic Filters

## Global OUI Only
Show frames where the **source MAC is globally assigned**.

```wireshark
!(wlan.sa[0] & 0x02)
```

## Local / Randomized Source Only
Show frames where the **source MAC is locally administered**.

```wireshark
wlan.sa[0] & 0x02
```

## Global Source and Destination Only
Show frames where **both source and destination** are globally assigned.

```wireshark
!(wlan.sa[0] & 0x02) && !(wlan.da[0] & 0x02)
```

## Local Source and Destination Only
Show frames where **both source and destination** are locally administered.

```wireshark
(wlan.sa[0] & 0x02) && (wlan.da[0] & 0x02)
```
---

# Management and Discovery Filters

## Probe Requests Only
Show only **probe requests**.

```wireshark
wlan.fc.type_subtype == 0x0004
```

## Probe Requests from Local / Randomized MACs
Useful for spotting randomized clients actively searching.

```wireshark
wlan.fc.type_subtype == 0x0004 && (wlan.sa[0] & 0x02)
```

## Probe Requests from Global MACs
Show probe requests from non-randomized sources.

```wireshark
wlan.fc.type_subtype == 0x0004 && !(wlan.sa[0] & 0x02)
```

## Probe Responses Only
Show only **probe responses**.

```wireshark
wlan.fc.type_subtype == 0x0005
```

## Beacon Frames Only
Show only **beacon frames**.

```wireshark
wlan.fc.type_subtype == 0x0008
```

## Action Frames Only
Show **802.11 action management** frames.

```wireshark
wlan.fc.type == 0 && wlan.fc.type_subtype == 13
```

## Public Action Frames
Show **public action** frames.

```wireshark
wlan_mgt.fixed.category_code == 4
```

## GAS Initial Request
Useful for service-discovery / ANQP-style traffic.

```wireshark
wlan_mgt.fixed.category_code == 4 && wlan_mgt.fixed.action_code == 10
```

## Retry Frames Only
Show frames marked as retries.

```wireshark
wlan.fc.retry == 1
```

## Retries from a Specific Source
Replace the example MAC with your target.

```wireshark
wlan.sa == aa:bb:cc:dd:ee:ff && wlan.fc.retry == 1
```

---

# MAC and OUI Matching

## Exact MAC Match
Show traffic involving one exact MAC.

```wireshark
wlan.addr == aa:bb:cc:dd:ee:ff
```

## Exact Source MAC Match
Show traffic sent by one exact source MAC.

```wireshark
wlan.sa == aa:bb:cc:dd:ee:ff
```

## Exact Destination MAC Match
Show traffic sent to one exact destination MAC.

```wireshark
wlan.da == aa:bb:cc:dd:ee:ff
```

## Two Specific Devices Talking
Show frames involving both MACs.

```wireshark
wlan.addr == aa:bb:cc:dd:ee:ff && wlan.addr == 11:22:33:44:55:66
```

## Match One OUI
Match any address beginning with one prefix.

```wireshark
wlan.addr[0:3] == aa:bb:cc
```

## Match Multiple OUIs
Example filter matching several target prefixes.

```wireshark
wlan.addr[0:3] == b8:1e:a4 ||
wlan.addr[0:3] == d8:f3:bc ||
wlan.addr[0:3] == d0:39:57 ||
wlan.addr[0:3] == c0:35:32
```

---

# EAPOL and Authentication Filters

## EAPOL Only
Show only **EAPOL** frames.

```wireshark
eapol
```

## WPA Handshake Traffic
Show EAPOL frames typically involved in WPA/WPA2 handshakes.

```wireshark
eapol || wlan_rsna_eapol
```

## Authentication Frames
Show **802.11 authentication** frames.

```wireshark
wlan.fc.type_subtype == 0x000b
```

## Deauthentication Frames
Show **deauthentication** frames.

```wireshark
wlan.fc.type_subtype == 0x000c
```

## Disassociation Frames
Show **disassociation** frames.

```wireshark
wlan.fc.type_subtype == 0x000a
```

## Auth or Assoc Failure Hunting
Useful when reviewing unstable or rejected joins.

```wireshark
wlan.fc.type_subtype == 0x000b || wlan.fc.type_subtype == 0x0000 || wlan.fc.type_subtype == 0x0002 || wlan.fc.type_subtype == 0x000a || wlan.fc.type_subtype == 0x000c
```

## EAPOL for One Device
Replace the example MAC with your target.

```wireshark
eapol && wlan.addr == aa:bb:cc:dd:ee:ff
```

---

# Association and Reassociation Filters

## Association Requests
Show **association request** frames.

```wireshark
wlan.fc.type_subtype == 0x0000
```

## Association Responses
Show **association response** frames.

```wireshark
wlan.fc.type_subtype == 0x0001
```

## Reassociation Requests
Show **reassociation request** frames.

```wireshark
wlan.fc.type_subtype == 0x0002
```

## Reassociation Responses
Show **reassociation response** frames.

```wireshark
wlan.fc.type_subtype == 0x0003
```

## Any Association Workflow
Useful for watching join and roam behavior.

```wireshark
wlan.fc.type_subtype == 0x0000 ||
wlan.fc.type_subtype == 0x0001 ||
wlan.fc.type_subtype == 0x0002 ||
wlan.fc.type_subtype == 0x0003
```

## Association Traffic for One Client
Replace the example MAC with your target.

```wireshark
(wlan.fc.type_subtype == 0x0000 ||
 wlan.fc.type_subtype == 0x0001 ||
 wlan.fc.type_subtype == 0x0002 ||
 wlan.fc.type_subtype == 0x0003) &&
wlan.addr == aa:bb:cc:dd:ee:ff
```

---

# BSSID-Centric Hunting Filters

## Exact BSSID Match
Show all traffic for one BSSID.

```wireshark
wlan.bssid == aa:bb:cc:dd:ee:ff
```

## BSSID OUI Match
Show traffic where the BSSID begins with a target prefix.

```wireshark
wlan.bssid[0:3] == d4:6c:6d
```

## Clients Talking to a Specific BSSID
Useful for identifying all stations around one AP.

```wireshark
wlan.bssid == aa:bb:cc:dd:ee:ff && wlan.sa != aa:bb:cc:dd:ee:ff
```

## Local / Randomized Clients Around a BSSID
Useful for finding randomized clients joining or probing near one AP.

```wireshark
wlan.bssid == aa:bb:cc:dd:ee:ff && (wlan.sa[0] & 0x02)
```

## Association Traffic for One BSSID
Useful for reviewing join activity on one AP.

```wireshark
wlan.bssid == aa:bb:cc:dd:ee:ff &&
(
 wlan.fc.type_subtype == 0x0000 ||
 wlan.fc.type_subtype == 0x0001 ||
 wlan.fc.type_subtype == 0x0002 ||
 wlan.fc.type_subtype == 0x0003
)
```

---

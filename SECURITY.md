# Security Policy 🔒 (URTC-SMART-RACK)

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 0.x.x  | ✅ Yes             |

## Reporting a Vulnerability

**CRITICAL: Do not report safety-critical vulnerabilities through public GitHub issues.**

In an intelligent tool rack, a security flaw can lead to hardware damage via thermal runaway or unauthorized tool state manipulation. If you discover a vulnerability affecting the **pre-heating logic**, **CAN-OTA bypasses**, or **ID spoofing**:

1. **Email**: Send a detailed report to `electrohobby3d@gmail.com`.
2. **Impact**: Describe if the bug allows bypassing thermal limits, corrupting tool usage logs in F-RAM, or hijacking the ATC coordination.
3. **Response**: Initial acknowledgment within 48 hours.

We follow a coordinated disclosure policy to ensure hardware safety before public release.

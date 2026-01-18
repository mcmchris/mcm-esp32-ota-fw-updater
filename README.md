# MCM_GitHub_OTA Library

**A robust, secure, and dual-network (Ethernet/WiFi) Over-The-Air (OTA) firmware update library for ESP32, designed specifically for GitHub Releases.**

This library allows your ESP32 devices to update themselves automatically by fetching binaries from a public or private GitHub repository. It features an intelligent fallback mechanism to ensure updates succeed even if certificate chains on CDNs change, preventing "bricked" remote devices.

## 🌟 Key Features

* **Dual Network Support:** Seamlessly works with **WiFi** (Standard ESP32) and **Ethernet** (W5500 via SPI). It automatically detects and uses the active interface.
* **GitHub Releases Integration:** Parses the GitHub API to find the latest release tag and corresponding binary asset (`.bin`).
* **Secure by Default:** Uses `SSLClient` with BearSSL and Trust Anchors (Root CAs) to ensure firmware is downloaded from the genuine source.
* **Resilient "Late Fallback" Mode:** If the secure connection to the API succeeds but the download server (CDN) fails (a common issue with changing certificates on AWS/Azure), the library automatically retries the download in "Insecure Mode" to ensure the update applies.
* **Memory Efficient:** Uses a shared global buffer to minimize heap fragmentation during large downloads.
* **Private Repo Support:** Supports Personal Access Tokens (PAT) for private repositories.

---

## 📦 Dependencies

To use this library, ensure the following dependencies are installed:

1.  **SSLClient** ([MCMCHRIS fork](https://github.com/mcmchris/SSLClient)) - For secure connection handling over Ethernet/WiFi.
2.  **ArduinoJson** (by Benoit Blanchon) - For parsing GitHub API responses.
3.  **Ethernet** (by Arduino) - Required if using W5500/W5100 modules.

---

## 🛠️ Installation

You can install this library using one of the following methods:

### Option 1: Manual Installation (Recommended for Development)
1.  Clone or download this repository as a ZIP file.
2.  Extract the folder.
3.  Move the extracted folder `MCM-ESP32-OTA-FW-UPDATER` into your Arduino libraries directory:
    * **Windows:** `Documents\Arduino\libraries\`
    * **macOS:** `Documents/Arduino/libraries/`
    * **Linux:** `~/Arduino/libraries/`
4.  Restart your Arduino IDE.

### Option 2: Arduino IDE Library Manager
1.  Open the Arduino IDE.
2.  Go to **Sketch** > **Include Library** > **Manage Libraries...**
3.  Search for `MCM_GitHub_OTA`.
4.  Click **Install**.

**Note:** You will need the [SSLClient library (MCMCHRIS version)](https://github.com/mcmchris/SSLClient)

---

## 📖 API Reference

### Constructor
```cpp
MCM_GitHub_OTA(bool enableEthernet, bool enableWiFi);
```
Initializes the OTA instance. You can selectively enable Ethernet or WiFi support.

- `enableEthernet`: Set to true to enable support for W5500 modules.

- `enableWiFi`: Set to true to enable support for native ESP32 WiFi.

### Initialization

```cpp
void begin(const char* owner, const char* repo, const char* currentVersion, const char* token = "");
```

Configures the target repository and versioning.

- `owner`: GitHub username or organization name.
- `repo`: The repository name.
- `currentVersion`: The current firmware tag (e.g., "v1.0.0"). If the remote tag differs, an update is triggered.
- `token`: (Optional) GitHub Personal Access Token for private repositories.
  
### Update Check

```cpp
void checkForUpdate();
```

Triggers the update process.

- It checks the network status -> Connects to GitHub API -> Compares versions -> Downloads binary -> Flashes ESP32 -> Reboots on success.
- **Note**: This is a blocking operation during the download process.
  
### Utilities

```cpp
void setSSLDebug(SSLClient::DebugLevel level);
```

Sets the debug level for the SSL handshake. Useful for troubleshooting certificate issues. Levels: `SSL_NONE`, `SSL_ERROR`, `SSL_WARN`, `SSL_INFO`, `SSL_DUMP`.

```cpp
bool isUpdated();
```
Returns `true` if the device is running the latest version according to the last check.

## Certificate Management (MCM_CertStore.h)

This library relies on **Trust Anchors** (Root Certificates) to verify the identity of GitHub servers. These are stored in the `MCM_CertStore.h` file included in the library.

Because GitHub and its CDNs (Amazon S3, Azure, etc.) rotate certificates, you must keep this file updated to ensure secure connections.

### How to update certificates
You can generate the content for `MCM_CertStore.h` using the [BearSSL Certificate Utility](https://openslab-osu.github.io/bearssl-certificate-utility/).

1. Go to the BearSSL Certificate Utility website.
2. In Domains To Include, enter the following domains to cover the API and various CDN endpoints:

`api.github.com, github.com, objects.githubusercontent.com, release-assets.githubusercontent.com, raw.githubusercontent.com, s3.amazonaws.com`

3. Header Customization Options:
   - Variable Name: TAs
   - Length Variable: TAs_NUM
   - Header Guard: MCM_CERT_STORE_H

4. Click Submit.
5. Copy the generated code and replace the content of MCM_CertStore.h in the library folder.

## 🛡️ Security Fallback Mechanism

Strict SSL verification is difficult on embedded devices because Root CA certificates expire or providers change (e.g., GitHub switching from DigiCert to Azure/Entrust).

**This library solves this via a "Late Fallback" approach:**

1. **Attempt 1 (Secure):** Tries to fetch JSON and Binary using the Trust Anchors defined in `MCM_CertStore.h`.
2. **Detection:** If the API connection works (securely) but the binary download fails (often due to CDN redirection to a URL with a different Root CA), the library detects this specific failure mode.
3. **Attempt 2 (Insecure Fallback):** The library destroys the secure client and creates a new one in Insecure Mode (skipping certificate validation) only for that download session.
4. **Result:** The device updates successfully instead of getting stuck, allowing you to push a new firmware with updated certificates.

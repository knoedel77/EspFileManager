#include "EspFileManager.h"
// #include "page.h"
#include "webPage.h"

//#define KNOEDEL_USES_THIS_FOR_DEBUGGING


EspFileManager::EspFileManager(/* args */) 
{
    _SDstorage = nullptr;
    _LittleFSstorage = nullptr;
    _currentStorage = nullptr;
    _server = nullptr;
}

EspFileManager::~EspFileManager() {}

/*
bool EspFileManager::initSDCard(fs::SDFS *storage, uint8_t _cs)
{
    sd_cs = _cs;
    _storage = storage;

    if (!_storage->begin(sd_cs))
    // if (!_storage->begin())
    {
        DEBUGLF("Card Mount Failed");
        memory_ready = false;
        return false;
    }

    uint8_t cardType = _storage->cardType();

    if (cardType == CARD_NONE)
    {
        DEBUGLF("No SD card attached");
        memory_ready = false;
        return false;
    }

    memory_ready = true;
    DEBUGF("SD Card Type: ");
    if (cardType == CARD_MMC)
    {
        DEBUGLF("MMC");
    }
    else if (cardType == CARD_SD)
    {
        DEBUGLF("SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
        DEBUGLF("SDHC");
    }
    else
    {
        DEBUGLF("UNKNOWN");
    }

    if(memory_ready)
    {
        // uint32_t cardSize = _storage->cardSize() / (1024 * 1024);
        // DEBUGX("SD Card Size: %lluMB\n", cardSize);
        // DEBUGL2("SD Card Size: ", cardSize);
    }
    return false;
}
*/

void EspFileManager::setSDFileSource(fs::SDFS *SDstorage)
{   
    _SDstorage = SDstorage;
    if(_currentStorage == nullptr)
       _currentStorage = SDstorage;
}

void EspFileManager::setLittleFSFileSource(fs::LittleFSFS *LittleFSstorage)
{   
    _LittleFSstorage = LittleFSstorage;
    if(_currentStorage == nullptr)
       _currentStorage = LittleFSstorage;
}

void EspFileManager::listDir(const char * dirname, uint8_t levels)
{
    DEBUGX("Listing directory: %s\n", dirname);

    if (_currentStorage == nullptr) {
        DEBUGLF("Storage is nullptr");
        return;
    }

    File root = _currentStorage->open(dirname);
    if(!root){
        DEBUGLF("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        DEBUGLF("Not a directory");
        return;
    }

    bool first_files = true;
    str_data = "";

    uint64_t totalSize = 0;
    uint64_t usedSize  = 0;

       // memory info for header
    if(_currentStorage == _SDstorage){
       totalSize = _SDstorage->totalBytes();
       usedSize  = _SDstorage->usedBytes();
    } else {
       totalSize = _LittleFSstorage->totalBytes();
       usedSize  = _LittleFSstorage->usedBytes();
    }

    str_data = "S,";
//    str_data += String((unsigned long)(totalSize / (1024ULL * 1024ULL)));
    str_data += String((unsigned long long)totalSize);
    str_data += ",";
//    str_data += String((unsigned long)(usedSize / (1024ULL * 1024ULL)));
    str_data += String((unsigned long long)usedSize);
    str_data += ":";   // Trennzeichen vor der Dateiliste

    File file = root.openNextFile();
    while(file){
        if (first_files)
            first_files = false;
        else 
            str_data += ":";

        if(file.isDirectory()){
            // DEBUGF("  DIR : ");
            // DEBUGL(file.name());
            str_data += "1,";
            str_data += file.name();
            // if(levels){
            //     listDir(file.path(), levels -1);
            // }
        } else {
            // DEBUG("  FILE: ");
            // DEBUG(file.name());
            // DEBUG("  SIZE: ");
            // DEBUGL(file.size());

            // Format: 0,<name>,<size>,<timestamp>
            str_data += "0,";
            str_data += file.name();
            str_data += ",";
            str_data += String(file.size());
            str_data += ",";

            // read last file mofification date/time and format it
            time_t t = file.getLastWrite();
            struct tm * tmstruct = localtime(&t);
            char buf[32];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tmstruct);
            str_data += buf;
        }
        file = root.openNextFile();
    }

    file.close();
    // DEBUGL2("Folder string ", str_data);
}

void EspFileManager::setServer(AsyncWebServer *server)
{
    if (server == nullptr) {
        DEBUGLF("Server is null!");
        return;
    }
    _server = server;
    
    _server->on("/file", AsyncWebRequestMethod::HTTP_GET, [&](AsyncWebServerRequest *request){ 
        if ((_httpUsername.length() > 0) && !request->authenticate(_httpUsername.c_str(), _httpPassword.c_str())){
            return request->requestAuthentication();
        }

        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", html_page, html_page_len);
#ifdef KNOEDEL_USES_THIS_FOR_DEBUGGING       
        // load HTML from LittleFS for development and debugging
        if (!LittleFS.exists("/file.html")) {
            request->send(404, "text/plain", "File not found");
            return;
        }
        request->send(LittleFS, "/file.html", "text/html");
#else
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
        // request->send(200, "text/html", html_page); 
        // request->send(200, "text/plain", "Test route working");
#endif

    });

    _server->on("/get-folder-contents", HTTP_GET, [&](AsyncWebServerRequest *request){
        if ((_httpUsername.length() > 0) && !request->authenticate(_httpUsername.c_str(), _httpPassword.c_str())){
            return request->requestAuthentication();
        }

        DEBUGL2("path:", request->arg("path").c_str());
        listDir(request->arg("path").c_str(), 0);
        request->send(200, "text/plain", str_data);
    });

    _server->on("/upload", HTTP_POST, [&](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"File upload complete\"}"); }, [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
    {
        if ((_httpUsername.length() > 0) && !request->authenticate(_httpUsername.c_str(), _httpPassword.c_str())){
            return request->requestAuthentication();
        }

        String file_path;

        // Zielverzeichnis aus dem Query-Parameter "path" lesen
        String upload_path = "/";
        if (request->hasArg("path")) {
            upload_path = request->arg("path");
        }

        // Sicherheitscheck: kein Path-Traversal
        if (upload_path.indexOf("..") >= 0) {
            DEBUGLF("Invalid upload path");
            return;
        }

        // Pfad normalisieren: muss mit "/" beginnen und enden
        if (!upload_path.startsWith("/")) upload_path = "/" + upload_path;
        if (!upload_path.endsWith("/"))   upload_path += "/";

        // Nur den reinen Dateinamen verwenden (falls der Browser einen Pfad mitsendet)
        int slashIndex = filename.lastIndexOf('/');
        if (slashIndex >= 0) {
            filename = filename.substring(slashIndex + 1);
        }

        file_path = upload_path + filename;

        if (!index)
        {
            DEBUGX("UploadStart: %s\n", file_path.c_str());
            if (_currentStorage->exists(file_path))
            {
                _currentStorage->remove(file_path);
            }
        }

        File file = _currentStorage->open(file_path, FILE_APPEND);
        if (file)
        {
            if (file.write(data, len) != len)
            {
                DEBUGLF("File write failed");
            }
            file.close();
        }
        if (final)
        {
            DEBUGX("UploadEnd: %s, %u B\n", file_path.c_str(), index + len);
        }
    });

    _server->on("/delete", HTTP_GET, [&](AsyncWebServerRequest *request){
        if ((_httpUsername.length() > 0) && !request->authenticate(_httpUsername.c_str(), _httpPassword.c_str())){
            return request->requestAuthentication();
        }

        String path;
        if (request->hasParam("path")) 
        {
            path = request->getParam("path")->value();
        } 
        else 
        {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Path not provided\"}");
            return;
        }

        DEBUGL2("Deleting File: ", path);
        if (_currentStorage->exists(path)) 
        {
            _currentStorage->remove(path);
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"File deleted successfully\"}");
        } 
        else 
        {
            request->send(404, "application/json", "{\"status\":\"error\",\"message\":\"File not found\"}");
        } 
    });

    _server->on("/download", HTTP_GET, [&](AsyncWebServerRequest *request){
        if ((_httpUsername.length() > 0) && !request->authenticate(_httpUsername.c_str(), _httpPassword.c_str())){
            return request->requestAuthentication();
        }

        String path;
        if (request->hasParam("path")) 
        {
            path = request->getParam("path")->value();
        } 
        else 
        {
            request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Path not provided\"}");
            return;
        }
        DEBUGL2("Downloading File: ", path);
        if (_currentStorage->exists(path)) 
        {
            request->send(*_currentStorage, path, String(), true);
        } 
        else 
        {
            request->send(404, "application/json", "{\"status\":\"error\",\"message\":\"File not found\"}");
        } 
    });

    _server->on("/create-folder", HTTP_GET, [&](AsyncWebServerRequest *request) {
        if ((_httpUsername.length() > 0) && !request->authenticate(_httpUsername.c_str(), _httpPassword.c_str())){
            return request->requestAuthentication();
        }

        if (!request->hasParam("path")) {
            request->send(400, "text/plain", "Missing path");
            return;
        }

        String path = request->getParam("path")->value();

        // check: path must begin with "/" and must not contain ".."
        if (!path.startsWith("/") || path.indexOf("..") >= 0) {
            request->send(400, "text/plain", "Invalid path");
            return;
        }

        if (_currentStorage->exists(path)) {
            request->send(409, "text/plain", "Already exists");
            return;
        }

        if (_currentStorage->mkdir(path)) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(500, "text/plain", "mkdir failed");
        }
    });

    server->on("/switch-storage", HTTP_GET, [this](AsyncWebServerRequest *request) {
        if ((_httpUsername.length() > 0) && !request->authenticate(_httpUsername.c_str(), _httpPassword.c_str())){
            return request->requestAuthentication();
        }
        
        if (!request->hasArg("fs")) {
            request->send(400, "text/plain", "Missing fs parameter");
            return;
        }

        String fs = request->arg("fs");
        if (fs == "sd" && _SDstorage != nullptr) {
            _currentStorage = _SDstorage;
            request->send(200, "text/plain", "OK");
        } else if (fs == "littlefs" && _LittleFSstorage != nullptr) {
            _currentStorage = _LittleFSstorage;
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "FS not available");
        } 
    });
}

void EspFileManager::setCredentials(const String& username, const String& password) {
    _httpUsername = username;
    _httpPassword = password;
}

void EspFileManager::printSDStorageInfo()
{
    if (_SDstorage == nullptr) {
        DEBUGLF("SD Storage is nullptr");
        return;
    }

    // physical card size
    uint64_t cardSize  = _SDstorage->cardSize();
    // size managed by file system
    uint64_t totalSize = _SDstorage->totalBytes();
    // occupied memory size
    uint64_t usedSize  = _SDstorage->usedBytes();
    // available memory size (computed by difference)
    uint64_t freeSize  = totalSize - usedSize;

    DEBUGX("SD Card Size : %llu MB\n", cardSize  / (1024ULL * 1024ULL));
    DEBUGX("Total space  : %llu MB\n", totalSize / (1024ULL * 1024ULL));
    DEBUGX("Used space   : %llu MB\n", usedSize  / (1024ULL * 1024ULL));
    DEBUGX("Free space   : %llu MB\n", freeSize  / (1024ULL * 1024ULL));
}

void EspFileManager::printLittleFSStorageInfo()
{
    if (_LittleFSstorage == nullptr) {
        DEBUGLF("SD Storage is nullptr");
        return;
    }

    // size managed by file system
    uint64_t totalSize = _LittleFSstorage->totalBytes();
    // occupied memory size
    uint64_t usedSize  = _LittleFSstorage->usedBytes();
    // available memory size (computed by difference)
    uint64_t freeSize  = totalSize - usedSize;

    DEBUGX("Total space  : %llu MB\n", totalSize / (1024ULL * 1024ULL));
    DEBUGX("Used space   : %llu MB\n", usedSize  / (1024ULL * 1024ULL));
    DEBUGX("Free space   : %llu MB\n", freeSize  / (1024ULL * 1024ULL));
}
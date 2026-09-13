#include "EspFileManager.h"
// #include "page.h"
#include "webPage.h"
#include "FS.h"
#include <SD.h>
#include "SPI.h"

//#define KNOEDEL_USES_THIS_FOR_DEBUGGING

#ifdef KNOEDEL_USES_THIS_FOR_DEBUGGING
// load HTML-page from LittleFS for development and debugging
#include <LittleFS.h>
#endif



EspFileManager::EspFileManager(/* args */) 
{
    _storage = nullptr;
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

void EspFileManager::setFileSource(fs::SDFS *storage)
{   
    _storage = storage;
}

void EspFileManager::listDir(const char * dirname, uint8_t levels)
{
    DEBUGX("Listing directory: %s\n", dirname);

    if (_storage == nullptr) {
        DEBUGLF("Storage is nullptr");
        return;
    }

    File root = _storage->open(dirname);
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

    // memory info for header
    uint64_t totalSize = _storage->totalBytes();
    uint64_t usedSize  = _storage->usedBytes();
    str_data = "S,";
    str_data += String((unsigned long)(totalSize / (1024ULL * 1024ULL)));
    str_data += ",";
    str_data += String((unsigned long)(usedSize / (1024ULL * 1024ULL)));
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
        DEBUGL2("path:", request->arg("path").c_str());
        listDir(request->arg("path").c_str(), 0);
        request->send(200, "text/plain", str_data);
    });

    _server->on("/upload", HTTP_POST, [&](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"File upload complete\"}"); }, [&](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
    {
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
            if (_storage->exists(file_path))
            {
                _storage->remove(file_path);
            }
        }

        File file = _storage->open(file_path, FILE_APPEND);
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

    server->on("/delete", HTTP_GET, [&](AsyncWebServerRequest *request){
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
        if (_storage->exists(path)) 
        {
            _storage->remove(path);
            request->send(200, "application/json", "{\"status\":\"success\",\"message\":\"File deleted successfully\"}");
        } 
        else 
        {
            request->send(404, "application/json", "{\"status\":\"error\",\"message\":\"File not found\"}");
        } 
    });

    server->on("/download", HTTP_GET, [&](AsyncWebServerRequest *request){
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
        if (_storage->exists(path)) 
        {
            request->send(*_storage, path, String(), true);
        } 
        else 
        {
            request->send(404, "application/json", "{\"status\":\"error\",\"message\":\"File not found\"}");
        } 
    });

    _server->on("/create-folder", HTTP_GET, [&](AsyncWebServerRequest *request) {
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

        if (_storage->exists(path)) {
            request->send(409, "text/plain", "Already exists");
            return;
        }

        if (_storage->mkdir(path)) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(500, "text/plain", "mkdir failed");
        }
    });
}

void EspFileManager::printStorageInfo()
{
    if (_storage == nullptr) {
        DEBUGLF("Storage is nullptr");
        return;
    }

    // physical card size
    uint64_t cardSize  = _storage->cardSize();
    // size managed by file system
    uint64_t totalSize = _storage->totalBytes();
    // occupied memory size
    uint64_t usedSize  = _storage->usedBytes();
    // available memory size (computed by difference)
    uint64_t freeSize  = totalSize - usedSize;

    DEBUGX("SD Card Size : %llu MB\n", cardSize  / (1024ULL * 1024ULL));
    DEBUGX("Total space  : %llu MB\n", totalSize / (1024ULL * 1024ULL));
    DEBUGX("Used space   : %llu MB\n", usedSize  / (1024ULL * 1024ULL));
    DEBUGX("Free space   : %llu MB\n", freeSize  / (1024ULL * 1024ULL));
}
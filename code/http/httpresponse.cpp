#include "httpresponse.h"

const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" },
};

int64_t HttpResponse::fd_ = 0;

HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
};

HttpResponse::~HttpResponse() {
    UnmapFile();
}

void HttpResponse::Init(const std::string& srcDir, std::string& path, bool isKeepAlive, int code){
    assert(srcDir != "");
    if(mmFile_) { UnmapFile(); }
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
}

void HttpResponse::MakeResponse(Buffer& buff) {
    /* 判断请求的资源文件 */
    // download
    std::regex patten(".*filedir.*");
    std::smatch subMatch;
    
    //下载
    if(regex_match(path_, subMatch, patten)) {
        std::filesystem::path filePath = path_;
        path_ = "filedir/" + filePath.filename().string();
    }
    if(path_ == "/docs.html") {
        ErrorHtml_();
        AddStateLine_(buff);
        AddHeader_(buff);
        AddFileListPage_();
        stat(filelistsrc.data(), &mmFileStat_);
        S_ISDIR(mmFileStat_.st_mode);
        AddContent_(buff);
    } else {
        if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
            code_ = 404;
        } else if(!(mmFileStat_.st_mode & S_IROTH)) {
            code_ = 403;
        } else if(code_ == -1) { 
            code_ = 200; 
        }
        ErrorHtml_();
        AddStateLine_(buff);
        AddHeader_(buff);
        AddContent_(buff);
    }
}

char* HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}

void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }
}

void HttpResponse::AddStateLine_(Buffer& buff) {
    std::string status;
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader_(Buffer& buff) {
    buff.Append("Connection: ");
    if(isKeepAlive_) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else {
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}

void HttpResponse::AddContent_(Buffer& buff) {
    std::cout << path_;
    std::string filepath_;
    if(path_ == "/docs.html") {
        filepath_ = filelistsrc;
    } else {
        filepath_ = srcDir_ + path_;
    }

    int srcFd = open(filepath_.data(), O_RDONLY);
    if(srcFd < 0) { 
        ErrorContent(buff, "File NotFound!");
        return; 
    }

    /* 将文件映射到内存提高文件的访问速度 
        MAP_PRIVATE 建立一个写入时拷贝的私有映射*/
    LOG_DEBUG("file path %s", filepath_.data());
    int* mmRet = (int*)mmap(0, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(*mmRet == -1) {
        ErrorContent(buff, "File NotFound!");
        return; 
    }
    mmFile_ = (char*)mmRet;
    close(srcFd);
    buff.Append("Content-length: " + std::to_string(mmFileStat_.st_size) + "\r\n\r\n");
    if(path_ == "/docs.html") {
        // std::remove(filelistsrc.c_str());
    }
}


void HttpResponse::UnmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

std::string HttpResponse::GetFileType_() {
    /* 判断文件类型 */
    std::string::size_type idx = path_.find_last_of('.');
    if(idx == std::string::npos) {
        return "text/plain";
    }
    std::string suffix = path_.substr(idx);
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}

void HttpResponse::ErrorContent(Buffer& buff, std::string message) {
    std::string body;
    std::string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += std::to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>Webserver</em></body></html>";

    buff.Append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}

void HttpResponse::getFileVec_(const std::string dirName, std::vector<std::string> &resVec) {
    DIR *dir;   // 目录指针
    dir = opendir(dirName.c_str());
    struct dirent *stdinfo;
    while (true) {
        // 获取文件夹中的一个文件
        stdinfo = readdir(dir);
        if (stdinfo == nullptr) {
            break;
        }
        resVec.push_back(stdinfo->d_name);
        if(resVec.back() == "." || resVec.back() == ".."){
            resVec.pop_back();
        }
    }
}

void HttpResponse::AddFileListPage_() {
    std::vector<std::string> fileVec;
    getFileVec_(srcDir_ + "filedir/", fileVec);
    
    // 结果保存到 resources/id_file.html
    filelistsrc = "resources/" + std::to_string(fd_) + "_filelist.html";
    std::ifstream inFile("resources/filelist.html", std::ios::in);
    std::ofstream outFile(filelistsrc);
    std::string tempLine;
    // 首先读取文件列表的 <!--filelist_label--> 注释前的语句
    int cnt = 67;
    while(cnt --){
        getline(inFile, tempLine);
        outFile << tempLine + "\n";
    }
    // 根据如下标签，将将文件夹中的所有文件项添加到返回页面中
    //             <tr><td class="col1">filenamename</td> <td class="col2"><a href="file/filename">下载</a></td> <td class="col3"><a href="delete/filename">删除</a></td></tr>
    for(const auto &filename : fileVec){
        outFile << "            <tr><td class=\"col1\">" + filename +
                    "</td> <td class=\"col2\"><a href=\"filedir/" + filename +
                    "\" download>下载</a></td> <td class=\"col3\"><a href=\"delete/" + filename +
                    "\" onclick=\"return confirmDelete();\">删除</a></td></tr>" + "\n";
    }
    // 将文件列表注释后的语句加入后面
    while(getline(inFile, tempLine)){
        outFile << tempLine + "\n";
    }
    inFile.close();
    outFile.close();
}

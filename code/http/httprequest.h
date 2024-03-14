#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <errno.h>     
#include <mysql/mysql.h>  //mysql
#include <iostream>
#include <fstream>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnRAII.h"

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,   // 请求行
        HEADERS,        // 请求头
        BODY,           // 请求体
        FINISH,         // 解析完成
    };

    enum HTTP_CODE {
        NO_REQUEST = 0,     // 无请求
        GET_REQUEST,        // GET请求
        BAD_REQUEST,        // 错误的请求
        NO_RESOURSE,        // 没有资源
        FORBIDDENT_REQUEST, // 禁止请求
        FILE_REQUEST,       // 文件请求
        INTERNAL_ERROR,     // 内容出错
        CLOSED_CONNECTION,  // 连接关闭
    };
    
    HttpRequest() { Init(); }
    ~HttpRequest() = default;

    void Init();
    int parse(Buffer& buff);

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

    bool IsKeepAlive() const;

    /* 
    todo 
    void HttpConn::ParseFormData() {}
    void HttpConn::ParseJson() {}
    */

private:
    bool ParseRequestLine_(const std::string& line);
    HTTP_CODE ParseHeader_(const std::string& line);
    HTTP_CODE ParseBody_(const std::string& line);

    void ParsePath_();
    void ParsePost_();
    void ParseFromUrlencoded_();
    void ParseFormData();

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);

    PARSE_STATE state_;
    std::string method_, path_, version_, body_;

    bool linger;
    size_t contentLen;

    std::unordered_map<std::string, std::string> header_;
    std::unordered_map<std::string, std::string> post_;
    std::unordered_map<std::string, std::string> fileInfo_;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
    static int ConverHex(char ch);
};


#endif //HTTP_REQUEST_H
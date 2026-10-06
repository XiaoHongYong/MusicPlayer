//
//  ApiHandler.hpp
//  MusicPlayer
//
//  本地 Web 管理后台的 REST API 处理器.
//
//  对应设计文档：docs/web-console/api.md、docs/web-console/plan.md §3.
//  挂在 URI 前缀 /api/v1 之下，返回 JSON，HTTP status 表达成功/失败.
//

#ifndef Http_ApiHandler_hpp
#define Http_ApiHandler_hpp

#include "IRequestHandler.hpp"

namespace HttpServer {

class ApiHandler : public IRequestHandler {
public:
    ApiHandler();

    virtual const std::string &getUriPath() const override { return m_uriPath; }
    virtual bool onRequestHeader(const ConnectionPtr &connection) override;
    virtual bool onRequestBody(const ConnectionPtr &connection) override;

protected:
    void handleRoute(const ConnectionPtr &connection, const std::vector<std::string> &tokens, const char *param);

    std::string                 m_uriPath;          // 本处理器挂载的前缀, 即 "/api/v1"
    std::string                 m_route;            // 去掉前缀后的子路径, 如 "/library/snapshot"
    std::string                 m_method;           // 当前请求方法
};

} // namespace HttpServer

#endif /* Http_ApiHandler_hpp */
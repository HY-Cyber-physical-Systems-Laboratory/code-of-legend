#pragma once
#include <drogon/HttpFilter.h>

namespace col {

class AuthFilter : public drogon::HttpFilter<AuthFilter>
{
public:
    void doFilter(const drogon::HttpRequestPtr &req,
                  drogon::FilterCallback &&cb,
                  drogon::FilterChainCallback &&ccb) override;
};

}

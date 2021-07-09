using System;
using Microsoft.AspNetCore.Http;

namespace AppMap {
    public class AspNetCore {
        public static RequestDelegate Wrap(RequestDelegate next) {
            return async context => {
                var req = context.Request;
                var call = HttpRequest(req.Method, req.Path);
                try {
                    await next(context);
                } finally {
                    HttpResponse(call, context.Response.StatusCode);
                }
            };
        }

        private static int HttpRequest(string method, string pathInfo) {
            // stub
            return 42;
        }

        private static void HttpResponse(int parentId, int status) {
            // stub
        }
    }
}

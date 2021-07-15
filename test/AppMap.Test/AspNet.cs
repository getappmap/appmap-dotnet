using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Builder;
using System;
using System.Threading.Tasks;
using Xunit;

namespace AppMap.Test
{
    namespace Code {
        class AspNetApplication {
            static public async Task Invoke(HttpContext context) {
                context.Response.StatusCode = 442;
                await context.Response.WriteAsync("Hello, World!");
            }
        }
    }

    class EmptyServiceProvider : IServiceProvider
    {
        public static EmptyServiceProvider Instance { get; } = new EmptyServiceProvider();
        public object? GetService(Type serviceType) => null;
    }

    public class AspNetTest {
        [Fact]
        static public void Builder()
        {
            var builder = new ApplicationBuilder(EmptyServiceProvider.Instance);
            builder.Run(Code.AspNetApplication.Invoke);
            var app = builder.Build();

            var context = new DefaultHttpContext();
            context.Request.Method = "POST";
            context.Request.Path = "/test/here?query=string";

            app.Invoke(context);
        }
    }
}

#include "x86_sse_mdct_common.c"

void
sse3_mdct_close(A52Context *ctx)
{
    ctx_close(&ctx->mdct_ctx_512);
    ctx_close(&ctx->mdct_ctx_256);
}

void
sse3_mdct_init(A52Context *ctx)
{
    ctx_init(&ctx->mdct_ctx_512, 512);
    ctx_init(&ctx->mdct_ctx_256, 256);

    ctx->mdct_ctx_512.mdct = mdct_512;
    ctx->mdct_ctx_256.mdct = mdct_256;

    ctx->mdct_ctx_512.mdct_close = sse3_mdct_close;
    ctx->mdct_ctx_256.mdct_close = sse3_mdct_close;
}

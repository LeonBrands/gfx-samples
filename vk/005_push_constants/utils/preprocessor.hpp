#pragma once

// a convenience macro for checking vulkan result values
// throws if the result from the expression is not VK_SUCCESS
// to reduce cost, we can simply run the expression in release mode
#ifdef NDEBUG
#define THROW_IF_FAILED(expr) expr;
#else
#define THROW_IF_FAILED(expr) if ((expr) != VK_SUCCESS) { printf("Vulkan expression %s failed", (#expr)); throw; }
#endif

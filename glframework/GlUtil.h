#pragma once

#include <cstdio>
#include <utility>

#include <glad/glad.h>

/// 轻量的 GL 资源与错误辅助。
///
/// 设计目标有两个：
///   1. 让离屏渲染、覆盖层这类“临时 GL 对象”在异常/提前返回时也能释放（move-only 句柄）；
///   2. 在 Debug 构建里把静默的 GL 错误变成带位置的日志行，而不是等到画面全黑才排查。
namespace gl
{
/// 排空 GL 错误队列。Debug 下逐条打印，发布构建里是空操作。
#ifdef NDEBUG
inline bool checkError(const char*)
{
    return true;
}
#else
inline bool checkError(const char* where)
{
    bool clean = true;
    for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
    {
        std::fprintf(stderr, "[GL] error 0x%04X at %s\n", static_cast<unsigned int>(error), where);
        clean = false;
    }
    return clean;
}
#endif

/// move-only 的 GL 对象基类：析构即删除，句柄不会被复制出两份。
template <typename DeleteFunction>
class Handle
{
public:
    explicit Handle(DeleteFunction deleter) : m_deleter(deleter) {}
    ~Handle() { reset(); }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    Handle(Handle&& other) noexcept : m_id(std::exchange(other.m_id, 0)), m_deleter(other.m_deleter) {}
    Handle& operator=(Handle&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            m_id = std::exchange(other.m_id, 0);
            m_deleter = other.m_deleter;
        }
        return *this;
    }

    /// 立即删除并清零，可重复调用。
    void reset()
    {
        if (m_id != 0)
        {
            m_deleter(m_id);
        }
        m_id = 0;
    }

    GLuint id() const { return m_id; }
    bool valid() const { return m_id != 0; }
    void assign(GLuint id) { reset(); m_id = id; }
    GLuint* address() { reset(); return &m_id; }

private:
    GLuint m_id = 0;
    DeleteFunction m_deleter;
};

struct TextureDeleter
{
    void operator()(GLuint id) const { glDeleteTextures(1, &id); }
};
struct FramebufferDeleter
{
    void operator()(GLuint id) const { glDeleteFramebuffers(1, &id); }
};
struct RenderbufferDeleter
{
    void operator()(GLuint id) const { glDeleteRenderbuffers(1, &id); }
};
struct BufferDeleter
{
    void operator()(GLuint id) const { glDeleteBuffers(1, &id); }
};
struct VertexArrayDeleter
{
    void operator()(GLuint id) const { glDeleteVertexArrays(1, &id); }
};

using Texture = Handle<TextureDeleter>;
using Framebuffer = Handle<FramebufferDeleter>;
using Renderbuffer = Handle<RenderbufferDeleter>;
using Buffer = Handle<BufferDeleter>;
using VertexArray = Handle<VertexArrayDeleter>;
}

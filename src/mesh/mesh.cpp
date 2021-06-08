#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol_gfx.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

int
main()
{
  /* create window and GL context via GLFW */
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow* Window = glfwCreateWindow(640, 480, "Sokol Triangle GLFW", 0, 0);
  glfwMakeContextCurrent(Window);
  glfwSwapInterval(1);

  /* setup sokol_gfx */
  sg_desc SgDesc{};
  sg_setup(&SgDesc);

  /* a vertex buffer */
  const float Vertices[] = {
    // positions            // colors
     0.0f,  0.5f, 0.5f,     1.0f, 0.0f, 0.0f, 1.0f,
     0.5f, -0.5f, 0.5f,     0.0f, 1.0f, 0.0f, 1.0f,
    -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 1.0f, 1.0f
  };
  sg_buffer_desc SgBufferDesc{.data = SG_RANGE(Vertices)};
  sg_buffer Vbuf = sg_make_buffer(&SgBufferDesc);

  /* a shader */
  sg_shader_desc SgShaderDesc{
    .vs{
      .source =
        "#version 330\n"
        "layout(location=0) in vec4 position;\n"
        "layout(location=1) in vec4 color0;\n"
        "out vec4 color;\n"
        "void main() {\n"
        "  gl_Position = position;\n"
        "  color = color0;\n"
        "}\n"
    },
    .fs{
      .source =
        "#version 330\n"
        "in vec4 color;\n"
        "out vec4 frag_color;\n"
        "void main() {\n"
        "  frag_color = color;\n"
        "}\n"
    }
  };
  sg_shader Shader = sg_make_shader(&SgShaderDesc);

  /* a pipeline state object (default render states are fine for triangle) */
  sg_pipeline_desc SgPipelineDesc{
    .shader = Shader,
    .layout = {
      .attrs = {
        sg_vertex_attr_desc{.format=SG_VERTEXFORMAT_FLOAT3},
        sg_vertex_attr_desc{.format=SG_VERTEXFORMAT_FLOAT4}
      }
    }
  };
  sg_pipeline Pipeline = sg_make_pipeline(&SgPipelineDesc);

  /* resource bindings */
  sg_bindings Binding = {.vertex_buffers = { Vbuf }};

  /* default pass action (clear to grey) */
  sg_pass_action PassAction{};

  /* draw loop */
  while (!glfwWindowShouldClose(Window)) {
    int CurWidth, CurHeight;
    glfwGetFramebufferSize(Window, &CurWidth, &CurHeight);
    sg_begin_default_pass(&PassAction, CurWidth, CurHeight);
    sg_apply_pipeline(Pipeline);
    sg_apply_bindings(&Binding);
    sg_draw(0, 3, 1);
    sg_end_pass();
    sg_commit();
    glfwSwapBuffers(Window);
    glfwPollEvents();
  }

  /* cleanup */
  sg_shutdown();
  glfwTerminate();
  return 0;
}

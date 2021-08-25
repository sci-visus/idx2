#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include "sokol_gfx.h"
#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

/* We "archive" the GUI and graphics code here */
void
OldMain()
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
}

#define IDX2_IMPLEMENTATION
#include "../idx2.hpp"
using namespace idx2;

struct edge {
  idx2::v3<u8> Start3; // starting vertex
  u8 DirAndLen; // last 2 bits indicate X/Y/Z, first 6 bits indicate the length
};

// Octree node
struct node {
  u8 Config = 0;
};

//idx2_T(t)
struct brick_data {
//  t* Coefficients = nullptr;
//  hash_table<u32, t> Vertices; // small vertex hashtable
  hash_table<u32, node> Nodes; // octree nodes stored using location codes
  // TODO: make sure the data is initialized correctly
};

/* Global hashtable to manage bricks at all levels */
struct brick_registry {
  hash_table<u64, brick_data> BrickTable;
};

struct block_data {
  hash_table<u32, node> Nodes;
};

struct mesh {
  hash_table<u32, block_data> Blocks;
};

// a single wavelet coefficient
idx2_T(t)
struct wave_coeff {
  u64 Pos = 0; // position (packed X/Y/Z)
  u8 Subband = 0; // TODO: technically the subband can be computed from the position
  t Val = 0;
};

enum recursive_update : u8 { Yes, No };
/* Update a node from a wavelet coefficient
 * Recursive update means we also updates the descendant nodes */
idx2_T(t) void
UpdateNode(node* Node, const wave_coeff<t>& W, recursive_update = No) {
  // TODO: change the node's config
  // TODO: update the vertices' values depending on the cofficient stencil
}

/* Update a mesh from a single wavelet coefficient */
idx2_T(t) void
UpdateMesh(mesh* Mesh, const wave_coeff<t>& W) {
  // TODO: query for the neighboring nodes to be updated
  // TODO: update each neighboring node, potentially recursively
  //
}

/* Update a mesh from an array of wavelet coefficients */
idx2_T(t) void
UpdateMesh(mesh* Mesh, const array<wave_coeff<t>>& Coeffs) {
  idx2_ForEach(W, Coeffs) {
    UpdateMesh(Mesh, *W);
  }
}

/* Read wavelet coefficients from an idx2 file */
void
ReadCoefficients()
{
  // Call the Decode function in idx_v1 but request only the wavelet coefficients
}

/* Read a .raw field, compute wavelet coefficients per brick, then set some to 0 */
void
FilterCoefficients()
{

}

/* Splat the wavelet coefficients to create vertices */
void
SplatCoefficients(const v3i& BrickDims3, brick_data* Brick)
{
//  const v3i TrueBrickDims3 = BrickDims3 + 1;
//  v3i P3;
//  for (P3.Z = 0; P3.Z < TrueBrickDims3.z; ++P3.Z) {
//  for (P3.Y = 0; P3.Y < TrueBrickDims3.y; ++P3.Y) {
//  for (P3.X = 0; P3.X < TrueBrickDims3.x; ++P3.X) {
//    i32 I = idx2::Row(BrickDims3, P3);
//    if (Brick->Coefficients[I] == 0) continue;

//  }}}
  // TODO: remember to copy to children bricks
  // Read from Brick->Coefficients and write to Brick->Vertices
  // TODO: also output a set of edges
  //
}

/* Extract cells from the vertices */
void
VerticesToCells(brick_data* Brick)
{
  // Make one pass through the buffer and gather all the vertex positions in an array
  // Go through that array and construct a k-d tree of cells (each leaf has 8 vertices)
  // NOTE: sometimes we have cells with more or less than 8 vertices (T-junctions and vertices that need to be created hat this step)
}

// TODO: IMPORTANT: once the list of vertices is computed, we need to go to the parent to fetch the vertices' values
// TODO: how to query vertex neighbors?
// TODO: how to query cell neighbors?
// TODO: inverse transform to regular grid (half done already in idx_v1)
// TODO: given a point, locate the smallest enclosing cell
// TODO: enumerate the cells
// TODO: how to "propagate" boundary vertices from one brick to another

/* Main */
int
main()
{
  return 0;
}

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#define CINTERFACE

#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../menu_driver.h"

#include "../../retroarch.h"
#include "../../gfx/font_driver.h"
#include "../../gfx/video_driver.h"
#include "../../gfx/common/d3d12_common.h"

static const float* menu_display_d3d12_get_default_vertices(void) { return NULL; }

static const float* menu_display_d3d12_get_default_tex_coords(void) { return NULL; }

static void* menu_display_d3d12_get_default_mvp(void) { return NULL; }

static void menu_display_d3d12_blend_begin(void)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)video_driver_get_ptr(false);

   /*todo: d3d12->sprites.pipe_blend */
   D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe);
}

static void menu_display_d3d12_blend_end(void)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)video_driver_get_ptr(false);

   /*todo: d3d12->sprites.pipe_noblend */
   D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe);
}

static void menu_display_d3d12_viewport(void* data) {}

static void menu_display_d3d12_draw(void* data)
{
   d3d12_video_t*           d3d12 = (d3d12_video_t*)video_driver_get_ptr(false);
   menu_display_ctx_draw_t* draw  = (menu_display_ctx_draw_t*)data;

   if (!d3d12 || !draw || !draw->texture)
      return;

   switch (draw->pipeline.id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         D3D12SetPipelineState(d3d12->queue.cmd, d3d12->pipes[draw->pipeline.id]);
         D3D12DrawInstanced(d3d12->queue.cmd, draw->coords->vertices, 1, 0, 0);
         D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe);
         D3D12IASetPrimitiveTopology(d3d12->queue.cmd, D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
         D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->sprites.vbo_view);
         return;
   }

   if (!d3d12->sprites.enabled)
      return;

   if (d3d12->sprites.offset + 1 > d3d12->sprites.capacity)
      d3d12->sprites.offset = 0;

   {
      d3d12_sprite_t* v;
      D3D12_RANGE     range = { 0, 0 };
      D3D12Map(d3d12->sprites.vbo, 0, &range, (void**)&v);
      v += d3d12->sprites.offset;

      v->pos.x = draw->x / (float)d3d12->chain.viewport.Width;
      v->pos.y = (d3d12->chain.viewport.Height - draw->y - draw->height) /
                 (float)d3d12->chain.viewport.Height;
      v->pos.w = draw->width / (float)d3d12->chain.viewport.Width;
      v->pos.h = draw->height / (float)d3d12->chain.viewport.Height;

      v->coords.u = 0.0f;
      v->coords.v = 0.0f;
      v->coords.w = 1.0f;
      v->coords.h = 1.0f;

      if (draw->scale_factor)
         v->params.scaling = draw->scale_factor;
      else
         v->params.scaling = 1.0f;

      v->params.rotation = draw->rotation;

      v->colors[3] = DXGI_COLOR_RGBA(
            0xFF * draw->coords->color[0], 0xFF * draw->coords->color[1],
            0xFF * draw->coords->color[2], 0xFF * draw->coords->color[3]);
      v->colors[2] = DXGI_COLOR_RGBA(
            0xFF * draw->coords->color[4], 0xFF * draw->coords->color[5],
            0xFF * draw->coords->color[6], 0xFF * draw->coords->color[7]);
      v->colors[1] = DXGI_COLOR_RGBA(
            0xFF * draw->coords->color[8], 0xFF * draw->coords->color[9],
            0xFF * draw->coords->color[10], 0xFF * draw->coords->color[12]);
      v->colors[0] = DXGI_COLOR_RGBA(
            0xFF * draw->coords->color[12], 0xFF * draw->coords->color[13],
            0xFF * draw->coords->color[14], 0xFF * draw->coords->color[15]);

      range.Begin = d3d12->sprites.offset * sizeof(*v);
      range.End   = (d3d12->sprites.offset + 1) * sizeof(*v);
      D3D12Unmap(d3d12->sprites.vbo, 0, &range);
   }

   {
      d3d12_texture_t* texture = (d3d12_texture_t*)draw->texture;
      if (texture->dirty)
         d3d12_upload_texture(d3d12->queue.cmd, texture);
      d3d12_set_texture_and_sampler(d3d12->queue.cmd, texture);
   }

   D3D12DrawInstanced(d3d12->queue.cmd, 1, 1, d3d12->sprites.offset, 0);
   d3d12->sprites.offset++;
   return;
}

static void menu_display_d3d12_draw_pipeline(void* data)
{
   menu_display_ctx_draw_t* draw  = (menu_display_ctx_draw_t*)data;
   d3d12_video_t*           d3d12 = (d3d12_video_t*)video_driver_get_ptr(false);

   if (!d3d12 || !draw)
      return;

   switch (draw->pipeline.id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      {
         video_coord_array_t* ca = menu_display_get_coords_array();

         if (!d3d12->menu_pipeline_vbo)
         {
            void*       vertex_data_begin;
            D3D12_RANGE read_range = { 0, 0 };

            d3d12->menu_pipeline_vbo_view.StrideInBytes = 2 * sizeof(float);
            d3d12->menu_pipeline_vbo_view.SizeInBytes =
                  ca->coords.vertices * d3d12->menu_pipeline_vbo_view.StrideInBytes;
            d3d12->menu_pipeline_vbo_view.BufferLocation = d3d12_create_buffer(
                  d3d12->device, d3d12->menu_pipeline_vbo_view.SizeInBytes,
                  &d3d12->menu_pipeline_vbo);

            D3D12Map(d3d12->menu_pipeline_vbo, 0, &read_range, &vertex_data_begin);
            memcpy(vertex_data_begin, ca->coords.vertex, d3d12->menu_pipeline_vbo_view.SizeInBytes);
            D3D12Unmap(d3d12->menu_pipeline_vbo, 0, NULL);
         }
         D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->menu_pipeline_vbo_view);
         draw->coords->vertices = ca->coords.vertices;
         break;
      }

      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->frame.vbo_view);
         draw->coords->vertices = 4;
         break;
      default:
         return;
   }
   D3D12IASetPrimitiveTopology(d3d12->queue.cmd, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   d3d12->ubo_values.time += 0.01f;

   {
      D3D12_RANGE read_range = { 0, 0 };
      d3d12_uniform_t* mapped_ubo;
      D3D12Map(d3d12->ubo, 0, &read_range, (void**)&mapped_ubo);
      *mapped_ubo = d3d12->ubo_values;
      D3D12Unmap(d3d12->ubo, 0, NULL);
   }
   D3D12SetGraphicsRootConstantBufferView(
         d3d12->queue.cmd, ROOT_ID_UBO, d3d12->ubo_view.BufferLocation);
}

static void menu_display_d3d12_restore_clear_color(void) {}

static void menu_display_d3d12_clear_color(menu_display_ctx_clearcolor_t* clearcolor)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)video_driver_get_ptr(false);

   if (!d3d12 || !clearcolor)
      return;

   D3D12ClearRenderTargetView(
         d3d12->queue.cmd, d3d12->chain.desc_handles[d3d12->chain.frame_index], (float*)clearcolor,
         0, NULL);
}

static bool menu_display_d3d12_font_init_first(
      void**      font_handle,
      void*       video_data,
      const char* font_path,
      float       font_size,
      bool        is_threaded)
{
   font_data_t** handle     = (font_data_t**)font_handle;
   font_data_t*  new_handle = font_driver_init_first(
         video_data, font_path, font_size, true, is_threaded, FONT_DRIVER_RENDER_D3D12_API);
   if (!new_handle)
      return false;
   *handle = new_handle;
   return true;
}

menu_display_ctx_driver_t menu_display_ctx_d3d12 = {
   menu_display_d3d12_draw,
   menu_display_d3d12_draw_pipeline,
   menu_display_d3d12_viewport,
   menu_display_d3d12_blend_begin,
   menu_display_d3d12_blend_end,
   menu_display_d3d12_restore_clear_color,
   menu_display_d3d12_clear_color,
   menu_display_d3d12_get_default_mvp,
   menu_display_d3d12_get_default_vertices,
   menu_display_d3d12_get_default_tex_coords,
   menu_display_d3d12_font_init_first,
   MENU_VIDEO_DRIVER_DIRECT3D12,
   "menu_display_d3d12",
};

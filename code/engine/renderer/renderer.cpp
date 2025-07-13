function b32 Renderer_Init(renderer* Renderer) {
	return true;
}

function b32 Render_Frame(renderer* Renderer) {
	
	gdi_render_pass_begin_info RenderPassInfo = {
		.RenderTargetViews = { GDI_Get_View() },
		.ClearState = {
			.ShouldClear = true,
			.ClearColor = { { .F32 = { 1, 1, 0, 1 } }  }
		}
	};
	gdi_render_pass* RenderPass = GDI_Begin_Render_Pass(&RenderPassInfo);
	GDI_End_Render_Pass(RenderPass);

	GDI_Submit_Render_Pass(RenderPass);

	gdi_render_params RenderParams = {};
	GDI_Render(&RenderParams);
	
	return true;
}
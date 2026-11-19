#pragma once

enum class EWorldType : uint8
{
	None,
	Game,
	Editor,
	PIE,
	EditorPreview	// 애셋 에디터 프리뷰 전용 독립 월드 (FPreviewScene 소유)
};

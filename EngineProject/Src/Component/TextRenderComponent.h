#pragma once

#include "BillboardComponent.h"
#include "Text/Font.h"

class UTextRenderComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)

	REFLECT_START(ClassName)
		PROPERTY(Text)
		PROPERTY(TextSize)
	REFLECT_END()

public:
	UTextRenderComponent();
	virtual ~UTextRenderComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime) override;

	// 텍스트는 FTextRenderer가 따로 그리므로 렌더 패킷을 만들지 않음
	virtual void SubmitToRenderQueue(TQueue<FRenderPacket>& RenderQueue) override {}

	const FString& GetText() const { return Text; }
	void SetText(const FString& InText) { Text = InText; }

	UFont* GetFont() const { return Font; }
	void SetFont(UFont* InFont) { Font = InFont; }

	float GetTextSize() const { return TextSize; }
	void SetTextSize(float InTextSize) { TextSize = InTextSize; }

private:
	FString Text = "Text";
	UFont* Font = nullptr;
	float TextSize = 1.0f;
};

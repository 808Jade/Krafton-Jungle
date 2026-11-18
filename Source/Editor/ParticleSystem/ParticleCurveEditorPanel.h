#pragma once
#include "Editor/EditorUI/EditorPanel.h"
#include "Particles/ParticleCurve.h"
#include "Core/EngineString.h"
#include "Containers/Array.h"
#include "imgui.h"

class FParticleEditorContext;
class UParticleModule;
class UTexture2D;

// 파티클 에디터 ⑥ 커브 에디터. (Cascade의 Curve Editor에 해당)
// 모듈이 GetCurves로 내놓는 커브(예: Size Scale X/Y/Z, Color Scale R/G/B, Alpha Scale)를 그래프로 보여 주고
// 키 추가·이동·삭제, 탄젠트 편집, 보간 방식(자동·자동/제한·사용자·꺾임·선형·상수)을 바꾼다.
// 이미터 패널에서 커브가 있는 모듈을 고르면 현재 커브 탭에 자동으로 추가된다.
//
// 조작: 좌클릭 키 선택(Ctrl 추가 선택) · 드래그로 이동, Ctrl+좌클릭으로 곡선 위에 키 추가, Delete로 키 삭제,
//       빈 곳 좌드래그는 툴바의 모드(패닝/줌), 우·휠 드래그는 항상 패닝, 휠은 확대·축소, 키 우클릭은 값 입력
class FParticleCurveEditorPanel : public IEditorPanel
{
public:
	// InTabId: 소속 탭의 ID. 다른 파티클 탭의 커브 에디터와 창 ID가 겹치지 않게 이름 뒤에 붙인다.
	FParticleCurveEditorPanel(FParticleEditorContext& InContext, const FString& InTabId)
		: Context(InContext), PanelName("Curve Editor###CurveEditor_" + InTabId) {}

	bool Init() override;
	void Tick(float DeltaTime) override {}
	void OnRender() override;
	const char* GetPanelName() const override { return PanelName.c_str(); }

private:
	// 커브 에디터에 올린 모듈 하나. 모듈의 커브 여러 개(서브 커브)를 묶어 보여 준다.
	struct FTrack
	{
		// 참조(집약): 에셋의 LOD 레벨이 소유하는 모듈. 모듈이 지워지면 매 프레임 검사해서 트랙을 뺀다.
		UParticleModule* Module = nullptr;
		FString Label;
		// 서브 커브 보이기 (1·0). TArray<bool>은 std::vector<bool>이라 참조를 돌려줄 수 없어 uint8을 쓴다.
		TArray<uint8> SubVisible;
		// 트랙 전체 보이기. (Cascade 트랙 오른쪽 끝 상자) 끄면 서브 커브 설정과 상관없이 그래프에서 모두 숨긴다.
		bool bTrackVisible = true;
		// 서브 커브 하나가 그래프에 보이는지: 트랙과 서브 커브가 둘 다 켜져 있어야 한다.
		bool IsSubVisible(int32 Sub) const { return bTrackVisible && SubVisible.IsValidIndex(Sub) && SubVisible[Sub]; }
	};
	// 툴바 "현재 탭"으로 고르는 트랙 묶음. Default 탭은 지울 수 없다.
	struct FCurveTab
	{
		FString Name;
		TArray<FTrack> Tracks;
	};
	// 선택한 키 하나: 트랙 · 서브 커브 · 키 인덱스
	struct FKeyRef
	{
		int32 Track = -1;
		int32 Sub = -1;
		int32 Key = -1;
		bool operator==(const FKeyRef& O) const { return Track == O.Track && Sub == O.Sub && Key == O.Key; }
	};
	enum class EMouseMode : uint8 { Pan, Zoom };
	enum class EDrag : uint8 { None, Keys, ArriveTangent, LeaveTangent, Pan, Zoom };

	void LoadIcons();
	FCurveTab& CurrentTab() { return Tabs[CurrentTabIndex]; }
	// 지워진 모듈의 트랙을 빼고, 이미터 패널에서 새로 고른 모듈을 현재 탭에 더한다.
	void SyncTracks();
	// 편집 대상 트랙(이미터 패널·트랙 목록에서 고른 모듈)을 정한다. 바뀌면 다른 트랙의 선택 키를 지운다.
	void UpdateActiveTrack();
	// 편집 대상 트랙이 없으면 모든 트랙이 편집 대상이다.
	bool IsTrackActive(int32 TrackIndex) const { return ActiveTrack < 0 || TrackIndex == ActiveTrack; }
	// 트랙(모듈)마다 다른 색. 트랙 목록의 색 막대와 그래프 이름표에 써서 Size·Color처럼 같은 R/G/B 커브를 구분한다.
	static ImU32 GetTrackColor(int32 TrackIndex, float Alpha = 1.0f);
	bool IsModuleInAsset(const UParticleModule* Module) const;
	FInterpCurveFloat* GetCurve(const FKeyRef& Ref);
	const FParticleCurveRef* GetCurveRef(int32 Track, int32 Sub);

	void DrawToolbar();
	bool ToolButton(const char* Id, UTexture2D* Icon, const char* Label, const char* Tooltip, bool bActive = false, bool bEnabled = true);
	void DrawTrackList();
	void DrawGraph();
	void DrawGrid(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max);
	void DrawKeyPopup();

	// 보기 범위 맞춤. 대상 키가 없으면 0~1로 둔다.
	void FitView(bool bHorizontal, bool bVertical, bool bSelectedOnly);
	// 선택 키의 보간 방식·탄젠트 바꾸기
	void SetSelectedInterpMode(EInterpCurveMode Mode);
	void FlattenSelectedTangents();
	void StraightenSelectedTangents();
	void DeleteSelectedKeys();
	// 키 시간 연결(LinkGroup): Ref 키와 같은 그룹·같은 시간에 있는 다른 커브의 키를 OutKeys에 더한다. (중복 제외)
	void AppendLinkedKeys(const FKeyRef& Ref, TArray<FKeyRef>& OutKeys);
	// Sub 커브에 Time 키를 추가했을 때, 같은 그룹 커브에도 그 시간 키를 (현재 곡선 값으로) 추가한다.
	void AddLinkedPoints(int32 Track, int32 Sub, float Time);
	// 키 묶음을 지운다. 같은 커브 안 인덱스가 밀리지 않게 큰 인덱스부터 지우고, 커브마다 마지막 키는 남긴다.
	void RemoveKeys(TArray<FKeyRef> Keys);
	void MarkEdited();

	// 커브 좌표 ↔ 화면 좌표
	ImVec2 ToScreen(float X, float Y) const;
	float ToCurveX(float ScreenX) const;
	float ToCurveY(float ScreenY) const;
	// 키 화면 위치에서 탄젠트 손잡이까지의 화면 벡터
	ImVec2 TangentHandleOffset(float Tangent, bool bLeave) const;
	bool ShouldShowTangents(const FKeyRef& Ref, const FInterpCurvePointFloat& Point) const;

	// 참조(집약): 탭이 소유하는 공유 상태
	FParticleEditorContext& Context;
	FString PanelName;

	// 참조(집약): 툴바 아이콘. 텍스처는 UAssetManager가 소유하고 캐시한다.
	struct FIcons
	{
		UTexture2D* FitHorizontal = nullptr;
		UTexture2D* FitVertical = nullptr;
		UTexture2D* FitSelected = nullptr;
		UTexture2D* Pan = nullptr;
		UTexture2D* Zoom = nullptr;
		UTexture2D* Auto = nullptr;
		UTexture2D* AutoClamped = nullptr;
		UTexture2D* User = nullptr;
		UTexture2D* Break = nullptr;
		UTexture2D* Linear = nullptr;
		UTexture2D* Constant = nullptr;
		UTexture2D* Flatten = nullptr;
		UTexture2D* Straighten = nullptr;
		UTexture2D* ShowAll = nullptr;
		UTexture2D* CreateTab = nullptr;
		UTexture2D* DeleteTab = nullptr;
	};
	FIcons Icons;

	TArray<FCurveTab> Tabs;
	int32 CurrentTabIndex = 0;
	int32 NextTabNumber = 1;
	// 마지막으로 본 이미터 패널 선택. 바뀌었을 때만 자동 추가한다.
	UParticleModule* LastSelectedModule = nullptr;
	// 편집 대상 트랙 인덱스. (Cascade처럼 고른 트랙만 진하게 그리고 키를 잡을 수 있다) -1이면 모든 트랙이 대상이다.
	int32 ActiveTrack = -1;
	int32 LastSelectedCurveCount = -1;

	// 보이는 커브 범위. (가로 = 수명 비율 0~1)
	float ViewMinX = -0.05f, ViewMaxX = 1.05f;
	float ViewMinY = -0.1f, ViewMaxY = 1.2f;
	ImVec2 GraphMin{}, GraphMax{};

	EMouseMode MouseMode = EMouseMode::Pan;
	bool bShowAllTangents = false;

	TArray<FKeyRef> SelectedKeys;
	EDrag Drag = EDrag::None;
	FKeyRef DragTangentKey;
	// 키 우클릭 값 입력 팝업
	FKeyRef PopupKey;
	bool bOpenKeyPopup = false;
	// 우클릭이 드래그(패닝)였는지 구분한다.
	bool bRightDragged = false;
};

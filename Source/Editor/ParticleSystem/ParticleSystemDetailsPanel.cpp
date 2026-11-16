#include "EnginePCH.h"
#include "Editor/ParticleSystem/ParticleSystemDetailsPanel.h"
#include "Editor/ParticleSystem/ParticleEditorContext.h"

#include "Components/ParticleSystemComponent.h"

void FParticleSystemDetailsPanel::OnRender()
{
	bool bOpen = IsOpen();
	if (ImGui::Begin(GetPanelName(), &bOpen))
	{
		// 이미터를 선택했으면 이미터를, 아니면 파티클 시스템 전체를 보여 준다. (Cascade와 같은 규칙)
		if (Context.GetSelectedEmitterInstance())
			DrawEmitterProperties();
		else
			DrawParticleSystemProperties();
	}
	ImGui::End();

	SetOpen(bOpen);
}

void FParticleSystemDetailsPanel::DrawParticleSystemProperties()
{
	UParticleSystemComponent* Component = Context.GetPreviewComponent();
	if (!Component)
	{
		ImGui::TextDisabled("No particle system.");
		return;
	}

	if (ImGui::CollapsingHeader("Particle System", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Use Fixed Bounds", &Component->bUseFixedRelativeBoundingBox);
		ImGui::BeginDisabled(!Component->bUseFixedRelativeBoundingBox);
		ImGui::DragFloat3("Fixed Bounds Min", &Component->FixedRelativeBoundingBox.Min.X, 0.05f);
		ImGui::DragFloat3("Fixed Bounds Max", &Component->FixedRelativeBoundingBox.Max.X, 0.05f);
		ImGui::EndDisabled();
	}

	if (ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Paused", &Context.bSimulationPaused);
		ImGui::Text("Emitters: %d", Component->EmitterInstances.Num());
	}
}

void FParticleSystemDetailsPanel::DrawEmitterProperties()
{
	FParticleEmitterInstance* Instance = Context.GetSelectedEmitterInstance();

	if (ImGui::CollapsingHeader("Emitter", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Index: %d", Context.SelectedEmitterIndex);
		ImGui::Text("Active Particles: %d / %d", Instance->ActiveParticles, Instance->MaxActiveParticles);
		ImGui::Checkbox("Use Local Space", &Instance->bUseLocalSpace);
	}

	if (ImGui::CollapsingHeader("Bounds", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const FBox& Box = Instance->GetBoundingBox();
		ImGui::Text("Min: %.2f, %.2f, %.2f", Box.Min.X, Box.Min.Y, Box.Min.Z);
		ImGui::Text("Max: %.2f, %.2f, %.2f", Box.Max.X, Box.Max.Y, Box.Max.Z);
	}

	ImGui::Spacing();
	ImGui::TextDisabled("Module properties appear once the particle asset exists.");
}

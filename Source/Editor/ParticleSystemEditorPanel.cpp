#include "EnginePCH.h"
#include "ParticleSystemEditorPanel.h"

bool FParticleSystemEditorPanel::Init()
{
	return false;
}

void FParticleSystemEditorPanel::Tick(float DeltaTime)
{



}

void FParticleSystemEditorPanel::OnRender()
{
	for (auto& Panel : Panels) 
	{

		Panel->OnRender();

	}

}

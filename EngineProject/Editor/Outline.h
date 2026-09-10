#pragma once

class FOutline
{
public:
	void SetTarget(UPrimitiveComponent* InTarget) { Target = InTarget; }
	UPrimitiveComponent* GetTarget() const { return Target; }
	FMesh* GetMesh() const { return Target->GetMesh(); }
	const FMatrix& GetWorldMatrix() const { return Target->GetTransform()->GetWorldMatrix(); }
	const FVector& GetTargetScale() const { return Target->GetTransform()->Scale; }
private:
	UPrimitiveComponent* Target = nullptr;

};
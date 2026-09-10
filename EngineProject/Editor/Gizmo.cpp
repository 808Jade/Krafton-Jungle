#include "EnginePCH.h"
#include "Gizmo.h"
#include "Camera/CameraComponent.h"

static const FVector AxisDirs[3] = {
	FVector(1, 0, 0),
	FVector(0, 1, 0),
	FVector(0, 0, 1),
};

static const int AXIS_PLANE_YZ = 3;
static const int AXIS_PLANE_ZX = 4;
static const int AXIS_PLANE_XY = 5;
static const int AXIS_SCREEN = 6;   // 3축 자유 이동

static const float AxisLength = 1.5f;
static const float HitPixels = 12.0f;

void FGizmo::Update(const FRay& MouseRay, const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH, bool bMouseDown, UCameraComponent* InCameraComponent)
{
	CameraComponent = InCameraComponent;

	if (!Target) // 만약 현재 타겟이 없다면 
	{
		HoveredAxis = -1;
		DraggingAxis = -1;
		return;
	}

	if (DraggingAxis >= 0) // 만약 드래그 중인 축이 있다면
	{
		if (bMouseDown)
			UpdateDrag(MouseRay, MousePos);
		else
			EndDrag();
		return;
	}

	HoveredAxis = PickAxis(MousePos, ViewProj, ScreenW, ScreenH); // 현재 마우스가 올라간 기즈모 축이 있으면

	if (bMouseDown && HoveredAxis >= 0) // 마우스가 올라간 축이 있고 마우스가 눌렸으면
		BeginDrag(HoveredAxis, MouseRay, MousePos);


}

int FGizmo::PickAxis(const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH)
{
	if (Mode == EGizmoMode::Rotation)
		return PickRotationAxis(MousePos, ViewProj, ScreenW, ScreenH);

	return PickLinearAxis(MousePos, ViewProj, ScreenW, ScreenH);
}

int FGizmo::PickLinearAxis(const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH)
{
	FVector Origin = GetRenderLocation();

	FVector2 Center = WorldToScreen(Origin, ViewProj, ScreenW, ScreenH);
	FVector2 d = MousePos - Center;
	if (sqrtf(d.X * d.X + d.Y * d.Y) < 15.0f)
		return AXIS_SCREEN;

	int Best = -1;
	float BestDist = HitPixels;

	for (int i = 0; i < 3; ++i)
	{
		FVector2 Start = WorldToScreen(Origin, ViewProj, ScreenW, ScreenH);
		FVector2 End = WorldToScreen(Origin + GetAxisDirection(i) * AxisLength, ViewProj, ScreenW, ScreenH);

		float Dist = DistanceToSegment(MousePos, Start, End);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = i;
		}
	}
	return Best;
}

int FGizmo::PickRotationAxis(const FVector2& MousePos, const FMatrix& ViewProj, int ScreenW, int ScreenH)
{
	const FVector Origin = GetRenderLocation();
	const int Segments = 32;

	int Best = -1;
	float BestDist = HitPixels;

	for (int Axis = 0; Axis < 3; ++Axis)
	{
		FVector u = GetAxisDirection((Axis + 1) % 3);
		FVector v = GetAxisDirection((Axis + 2) % 3);

		FVector2 prev;
		bool bHasPrev = false;

		for (int s = 0; s <= Segments; ++s)
		{
			float theta = (float)s / Segments * 2.0f * PI;
			FVector worldPos = Origin
				+ u * (RingRadius * cosf(theta))
				+ v * (RingRadius * sinf(theta));

			FVector2 screenPos = WorldToScreen(worldPos, ViewProj, ScreenW, ScreenH);

			if (bHasPrev)
			{
				float Dist = DistanceToSegment(MousePos, prev, screenPos);
				if (Dist < BestDist)
				{
					BestDist = Dist;
					Best = Axis;
				}
			}

			prev = screenPos;
			bHasPrev = true;
		}
	}

	FVector CamPos = GetCameraLocation();
	FVector Forward = (Origin - CamPos).Normalize();
	FVector u = FVector(0, 0, 1).Cross(Forward).Normalize();
	FVector v = Forward.Cross(u);

	const float ScreenRingRadius = RingRadius * 1.3f;  

	FVector2 prev;
	bool bHasPrev = false;

	for (int s = 0; s <= Segments; ++s)
	{
		float theta = (float)s / Segments * 2.0f * PI;
		FVector worldPos = Origin
			+ u * (ScreenRingRadius * cosf(theta))
			+ v * (ScreenRingRadius * sinf(theta));

		FVector2 screenPos = WorldToScreen(worldPos, ViewProj, ScreenW, ScreenH);

		if (bHasPrev)
		{
			float Dist = DistanceToSegment(MousePos, prev, screenPos);
			if (Dist < BestDist)
			{
				BestDist = Dist;
				Best = AXIS_SCREEN;
			}
		}

		prev = screenPos;
		bHasPrev = true;
	}

	return Best;
}

void FGizmo::BeginDrag(int Axis, const FRay& MouseRay, const FVector2& MousePos)
{
	DragStartMousePos = MousePos;
	DraggingAxis = Axis;
	DragStartLocation = GetLocation();
	DragStartRenderLocation = GetRenderLocation();    
	DragStartRotation = GetRotation();
	DragStartScale = GetScale();

	DragAxisDirection = GetAxisDirection(Axis);
	FVector axis = DragAxisDirection;

	if (Axis == AXIS_SCREEN)
	{
		if (Mode == EGizmoMode::Rotation)
		{
			DragAxisDirection = (GetRenderLocation() - GetCameraLocation()).Normalize();
			DragPlaneNormal = DragAxisDirection;
		}
		else
		{
			DragPlaneNormal = -MouseRay.Direction;
		}
	}
	else if (Mode == EGizmoMode::Rotation)
	{
		FVector ToCamera = (GetCameraLocation() - GetRenderLocation()).Normalize();
		if (DragAxisDirection.Dot(ToCamera) < 0.0f)
			DragAxisDirection = -DragAxisDirection;

		DragPlaneNormal = DragAxisDirection;
	}
	else
	{
		FVector ToCamera = -MouseRay.Direction;
		FVector Ortho = axis.Cross(ToCamera);
		DragPlaneNormal = Ortho.Cross(axis).Normalize();
	}

	float t;
	if (RayIntersectsPlane(MouseRay, DragStartRenderLocation, DragPlaneNormal, t))
		DragStartPoint = MouseRay.Origin + MouseRay.Direction * t;
	else
		DragStartPoint = DragStartRenderLocation;

	if (Mode == EGizmoMode::Rotation)
		DragStartAngle = ComputeAngleOnPlane(DragStartPoint, Axis);
}

void FGizmo::UpdateDrag(const FRay& MouseRay, const FVector2& MousePos)
{
	float t;
	if (!RayIntersectsPlane(MouseRay, DragStartRenderLocation, DragPlaneNormal, t))
		return;

	FVector current = MouseRay.Origin + MouseRay.Direction * t;

	if (DraggingAxis == AXIS_SCREEN && Mode != EGizmoMode::Rotation)
	{
		if (Mode == EGizmoMode::Location)
			Target->GetTransform()->Location = DragStartLocation + (current - DragStartPoint);
		else
		{
			float dx = MousePos.X - DragStartMousePos.X;
			float dy = DragStartMousePos.Y - MousePos.Y;   // 화면 Y는 아래가 +
			float scaleDelta = (dx + dy) * 0.005f;

			float factor = 1.0f + scaleDelta;
			if (factor < 0.01f) factor = 0.01f;

			Target->GetTransform()->Scale = DragStartScale * factor;
		}
		return;
	}

	if (Mode == EGizmoMode::Rotation)
	{
		float currentAngle = ComputeAngleOnPlane(current, DraggingAxis);
		float deltaAngle = currentAngle - DragStartAngle;

		FQuat delta = FQuat::MakeFromAxisAngle(DragAxisDirection, deltaAngle);
		FQuat start = DragStartRotation.Quaternion();

		FQuat result = delta * start;

		Target->GetTransform()->Rotation = result.ToFRotator();
		return;
	}

	FVector delta = current - DragStartPoint;
	float amount = delta.Dot(DragAxisDirection);

	if (Mode == EGizmoMode::Location)
		Target->GetTransform()->Location = DragStartLocation + DragAxisDirection * amount;
	else   // Scale
	{
		float factor = 1.0f + amount;
		if (factor < 0.01f) factor = 0.01f;

		FVector NewScale = DragStartScale;
		if (DraggingAxis == 0)      NewScale.X *= factor;
		else if (DraggingAxis == 1) NewScale.Y *= factor;
		else                        NewScale.Z *= factor;

		Target->GetTransform()->Scale = NewScale;
	}
}

void FGizmo::EndDrag()
{
	DraggingAxis = -1;
}

float FGizmo::ComputeAngleOnPlane(const FVector& Point, int Axis) const
{
	FVector u, v;

	if (DraggingAxis >= 0)
	{
		FVector Ref = (fabsf(DragAxisDirection.Z) > 0.9f) ? FVector(1, 0, 0) : FVector(0, 0, 1);
		u = Ref.Cross(DragAxisDirection).Normalize();
		v = DragAxisDirection.Cross(u);
	}
	else if (Axis == AXIS_SCREEN)
	{
		FVector Forward = (GetRenderLocation() - GetCameraLocation()).Normalize();
		u = FVector(0, 0, 1).Cross(Forward).Normalize();
		v = Forward.Cross(u);
	}
	else
	{
		u = AxisDirs[(Axis + 1) % 3];
		v = AxisDirs[(Axis + 2) % 3];
	}

	FVector local = Point - DragStartRenderLocation;
	return atan2f(local.Dot(v), local.Dot(u));
}

FVector FGizmo::GetAxisDirection(int Axis) const
{
	if (Axis < 0 || Axis > 2) return FVector(0, 0, 0);
	bool bUseLocal = (Space == EGizmoSpace::Local) || (Mode == EGizmoMode::Scale);

	if (bUseLocal && Target)
	{
		FMatrix rot = Target->GetTransform()->Rotation.Quaternion().ToFMatrix();
		FVector4 v = rot.TransformVector(AxisDirs[Axis]);
		return FVector(v.X, v.Y, v.Z).Normalize();
	}
	return AxisDirs[Axis];
}

FVector FGizmo::GetRenderLocation() const
{
	if (!Target) return FVector(0, 0, 0);

	if (CameraComponent->bIsOrthogonal) return GetLocation();

	return (Target->GetTransform()->Location - CameraComponent->GetTransform()->Location).Normalize() * 10.0f + CameraComponent->GetTransform()->Location;

	// return Target ? Target->GetTransform()->Location: FVector(0, 0, 0); 
}

FVector FGizmo::GetCameraLocation() const
{
	return CameraComponent ? CameraComponent->GetTransform()->Location : FVector(0, 0, 0);
}

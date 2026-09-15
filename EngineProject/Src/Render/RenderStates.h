#pragma once

enum class ERasterizerState : uint8 { SolidBack, SolidNone, SolidFront, Wireframe, Count };
enum class EDepthStencilState : uint8 { Default, Disabled, Count };
enum class EBlendState : uint8 { Opaque, AlphaBlend, Count };
enum class ESamplerState : uint8 { LinearClamp, Count };


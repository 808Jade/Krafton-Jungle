# SubUV time / lifetime comparison

- Texture: `Showcase_FireBurst_4x4.png` (RGBA, transparent background).
- Layout: 4 columns x 4 rows, 16 frames, left to right then top to bottom.
- Material: `ShowcaseSubUVComparisonMaterial`, registered in `UAssetManager::CreateParticleMaterial()`.
- `Showcase_Sparks.particle`: SubUVMovie, 24 FPS, looping (one cycle every 2/3 second).
- `Showcase_Flipbook.particle`: SubUV, frames 0 through 15 over each particle's lifetime.
- Rebuild and restart the editor after the material registration change, then reopen `3.ParticleShowcase.scene`.

Generated and edited using the built-in ImageGen tool. The current texture removes the pale smoke masses and keeps orange flames, yellow hot cores, and sparks on transparent alpha. Only the PNG changed in this cleanup; the two playback configurations remain the same. Restart the editor to reload the cached texture (no additional code rebuild needed for this PNG change).

## Smoke removal prompt (current image)

Edit this VFX sprite atlas to REMOVE ALL SMOKE and all gray, white, beige, pink cloudy masses. Replace those pixels with FULL TRANSPARENCY. Keep only saturated orange flame tongues, small yellow hot flame cores, and orange sparks. Reinterpret if necessary as clean stylized 2D hand-painted FIRE ONLY animation rather than smoky photorealistic explosions. No large pale opaque interiors, no cloudy rim, no fog, no haze, no broad glow, no background or shadow. Critical: real transparent alpha throughout all gaps between flame tongues, even INSIDE each burst. Each sprite must have a sharply readable fiery orange silhouette with transparent empty holes and modest antialiasing. Maintain EXACT 4 columns and 4 rows of equal cells, same centered alignment and row-major sequence of 16 frames: small ignition, expanding orange curling fire burst, breaking apart into flame fragments, final isolated fading orange embers. Each sprite must fit comfortably inside central 65% of its cell with generous fully transparent margins. Render a square atlas, preferably 1024x1024. No frame labels, no drawn grid. This is a production texture displayed on a light cyan background, so gray or pale smoke would be clearly visible and unacceptable. ALL SMOKE MUST BE REMOVED, retain only fire and sparks.

## Initial prompt

Use case: stylized-concept. Asset type: game VFX SubUV sprite sheet. Generate a square 1024x1024 RGBA image with truly transparent background. EXACT 4 columns by 4 rows, 16 equal 256x256 cells, no gaps, no drawn grid. A single centered orange-gold magical fire burst evolving through 16 sequential frames in row-major order left to right top to bottom: tiny bright ignition in first cell, growing compact flame in cells 2-4, expanding curling fireball in cells 5-8, dissipating amber wisps and sparks in cells 9-12, sparse fading embers in cells 13-16. Each cell represents the SAME effect at a later moment. Keep every sprite centered in its own cell and within the central 80 percent of cell, with transparent margins. Distinct silhouette changes make animation speed clearly visible. Warm orange red outer flames and pale yellow core, painted game VFX style with crisp readable curling shapes, soft transparent edges. No smoke background, no black rectangles, no text, no frame numbers, no watermark, no scenery. Preserve actual alpha.

## Refinement prompt

Correct this game sprite sheet. Keep EXACT square 4x4 uniform layout and sequential orange fire burst animation, but make EACH of the 16 sprites MUCH SMALLER, fitting completely within central 55% width and 55% height of its individual cell. Every individual cell must have a wide completely transparent border on ALL FOUR sides. No fire, glow, sparks or smoke may cross cell boundaries. Sixteen separate isolated small sprites with large empty transparent spaces between them. True alpha transparency, no background. Fixed center positions: at 1/8,3/8,5/8,7/8 image width and height. No text or grid lines. Retain progression from ignition to expanding fire to fading embers. Use crisp painted curling flames and bounded soft edges. All 16 frames must be comfortably isolated from all neighbors. Make the 16 sprites all small! The largest sprite must fit in 55% of one cell.

#version 430 core

in vec2 TexCoord;
uniform sampler2D screenTexture;

// Ring mode parameters
uniform int renderMode;          // 0: centers, 1: rings
uniform float ringSize;          // Ring thickness (0.0 to 1.0)
uniform float selectionAlpha;    // Alpha multiplier for selected splats
uniform bool showOverlay;        // Show selection overlay

uniform vec4 selectedColor;      // Color for selected splats
uniform vec4 unselectedColor;    // Color for unselected splats
uniform vec4 lockedColor;        // Color for locked splats

out vec4 FragColor;

// Simple hash function for pseudo-random values
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Create a ring effect based on pixel intensity
float createRingMask(vec2 uv, float intensity) {
    if (intensity < 0.01) return intensity; // Skip dark pixels

    // Use pixel position to create a pseudo-splat center
    vec2 gridPos = floor(uv * 64.0); // 64x64 grid
    vec2 cellUV = fract(uv * 64.0);

    // Create pseudo-random splat centers within each cell
    vec2 splatCenter = vec2(hash(gridPos), hash(gridPos + vec2(1.0, 0.0)));
    splatCenter = splatCenter * 0.6 + 0.2; // Keep centers away from edges

    float distToCenter = length(cellUV - splatCenter);

    // Create ring effect
    float ringInner = 0.15;
    float ringOuter = 0.35;
    float ringThickness = ringSize * 0.3;

    float ringMask = 1.0;

    if (distToCenter < ringInner) {
        // Inside ring - reduce alpha
        ringMask = mix(0.1, 1.0, distToCenter / ringInner);
    } else if (distToCenter > ringOuter) {
        // Outside ring - fade out
        ringMask = max(0.1, 1.0 - (distToCenter - ringOuter) * 3.0);
    } else {
        // On the ring - enhance
        float ringPos = (distToCenter - ringInner) / (ringOuter - ringInner);
        ringMask = 1.0 + sin(ringPos * 3.14159) * 0.5;
    }

    return ringMask;
}

void main() {
    vec4 originalColor = texture(screenTexture, vec2(TexCoord.x, 1.0 - TexCoord.y));

    vec3 finalColor = originalColor.rgb;
    float finalAlpha = originalColor.a;

    // Apply ring mode effects
    if (renderMode == 1 && ringSize > 0.0) { // Ring mode
        float intensity = length(originalColor.rgb);

        if (intensity > 0.05) { // Only apply to visible pixels
            float ringMask = createRingMask(TexCoord, intensity);

            // Apply ring effect to alpha
            finalAlpha *= ringMask;

            // Enhance ring visibility
            if (showOverlay) {
                // Add subtle color tinting for rings
                vec3 ringTint = mix(vec3(1.0), selectedColor.rgb, 0.2);
                finalColor *= ringTint;
            }
        }
    }

    // Apply selection alpha
    finalAlpha *= selectionAlpha;

    // Color modifications based on mode
    if (showOverlay && renderMode == 1) {
        // In ring mode with overlay, slightly tint towards selection color
        finalColor = mix(finalColor, selectedColor.rgb, selectedColor.a * 0.1);
    }

    FragColor = vec4(finalColor, finalAlpha);
}
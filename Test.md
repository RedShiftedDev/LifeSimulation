// #version 410 core

// in vec2 TexCoord;
// in vec4 Color;
// in float Active;
// out vec4 FragColor;

// uniform float uTime;
// uniform sampler2D uNoiseTexture;  // Noise texture for organic patterns
// uniform vec3 uAccentColor;        // Secondary color for harmonious blending

// void main() {
//     // Discard inactive particles
//     if (Active < 0.5) 
//         discard;
    
//     // Calculate distance from center
//     vec2 center = vec2(0.5, 0.5);
//     float dist = length(TexCoord - center) * 2.0;
    
//     // Create organic noise patterns
//     vec2 noiseCoord = TexCoord * 3.0 + vec2(uTime * 0.1, uTime * 0.08);
//     float noise = texture(uNoiseTexture, noiseCoord).r * 0.2;
    
//     // Create fractal patterns
//     float fractal = 0.0;
//     float amplitude = 1.0;
//     float frequency = 1.0;
//     for (int i = 0; i < 4; i++) {
//         fractal += amplitude * texture(uNoiseTexture, noiseCoord * frequency).r;
//         amplitude *= 0.5;
//         frequency *= 2.0;
//     }
//     fractal = fractal * 0.15;
    
//     // Dynamic pulsation
//     float pulseSpeed = 0.8 + 0.2 * sin(uTime * 0.3);
//     float pulseMagnitude = 0.15;
//     float pulse = 1.0 + pulseMagnitude * sin(uTime * pulseSpeed);
    
//     // Create a more interesting shape with noise-based distortion
//     float distortedDist = dist - noise * 0.4 - fractal * 0.3;
    
//     // Create light ray effects emanating from center
//     float rays = 0.5 + 0.5 * sin(atan(TexCoord.y - 0.5, TexCoord.x - 0.5) * 8.0 + uTime * 2.0);
//     rays *= smoothstep(1.0, 0.0, dist) * 0.15;
    
//     // Color manipulation for beautiful gradients
//     vec3 baseColor = Color.rgb;
//     vec3 accentColor = uAccentColor;
    
//     // Create internal glow with vibrant center
//     float innerGlow = smoothstep(1.0, 0.0, dist * 2.0);
//     vec3 glowColor = mix(baseColor * 1.6, accentColor, 0.5) * innerGlow;
    
//     // Create outer ethereal glow
//     float outerGlow = exp(-dist * 3.0) * 0.6;
    
//     // Blend colors using the distorted distance
//     float colorMix = smoothstep(0.0, 0.8, distortedDist);
//     vec3 gradientColor = mix(baseColor * 1.3, accentColor * 0.7, colorMix);
    
//     // Apply vignette effect for depth
//     float vignette = smoothstep(1.4 * pulse, 0.4 * pulse, distortedDist);
    
//     // Combine all effects
//     vec3 finalColor = gradientColor;
//     finalColor += glowColor;
//     finalColor += outerGlow * accentColor;
//     finalColor += rays * accentColor;
//     finalColor *= vignette;
    
//     // Apply subtle color aberration
//     float aberration = 0.03;
//     vec3 rgbOffset = vec3(
//         smoothstep(0.8 * pulse - aberration, 1.0 * pulse - aberration, distortedDist),
//         smoothstep(0.8 * pulse, 1.0 * pulse, distortedDist),
//         smoothstep(0.8 * pulse + aberration, 1.0 * pulse + aberration, distortedDist)
//     );
//     finalColor = mix(finalColor, finalColor * rgbOffset, 0.3);
    
//     // Edge softness with dynamic pulse
//     float alpha = smoothstep(1.0 * pulse, 0.8 * pulse, distortedDist);
//     alpha = mix(alpha, alpha * (0.9 + 0.1 * sin(uTime * 3.0 + dist * 5.0)), 0.3);
    
//     // Discard pixels outside the main shape
//     if (distortedDist > 1.0 * pulse) 
//         discard;
    
//     // Output final color with all effects applied
//     FragColor = vec4(finalColor, alpha * Color.a);
// }

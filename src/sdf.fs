#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Обводка текста: цвет и ширина (в единицах SDF-дистанции, ~0..0.5).
// При outlineWidth <= 0 обводка отключена — поведение как раньше.
uniform vec4 outlineColor;
uniform float outlineWidth;

// Output fragment color
out vec4 finalColor;

void main()
{
    // Знаковая дистанция до края глифа (положительная внутри)
    float distanceFromOutline = texture(texture0, fragTexCoord).a - 0.5;
    float distanceChangePerFragment = length(vec2(dFdx(distanceFromOutline), dFdy(distanceFromOutline)));
    float fillAlpha = smoothstep(-distanceChangePerFragment, distanceChangePerFragment, distanceFromOutline);

    if (outlineWidth <= 0.0) {
        finalColor = vec4(fragColor.rgb, fragColor.a * fillAlpha);
    } else {
        // Внешний контур: та же дистанция, сдвинутая наружу на outlineWidth.
        // Полоса перехода края обводки не должна доставать до углов квада
        // глифа (d ≈ -0.5, alpha атласа = 0), иначе при zoom-out весь квад
        // «загорается» и получаются квадраты. aa заливки остаётся экранно-
        // корректным — ограничиваем только внешний край.
        float aaOuter = min(distanceChangePerFragment, max(0.0, 0.5 - outlineWidth - 0.03));
        float outerAlpha = smoothstep(-aaOuter, aaOuter, distanceFromOutline + outlineWidth);
        vec3 rgb = mix(outlineColor.rgb, fragColor.rgb, fillAlpha);
        float a = mix(outlineColor.a, fragColor.a, fillAlpha) * outerAlpha;
        finalColor = vec4(rgb, a);
    }
}

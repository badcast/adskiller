#!/usr/bin/env python3
"""
Generate vector SVG icons and convert them to PNG and ICO for AdsKiller.
- Application Icon: res/icon.svg -> res/icon.png, res/icon_std.png, res/icon.ico
- Update Icon: res/icon_update.svg -> res/icon_update.png, res/icon_update.ico
"""

import os
import subprocess
from PIL import Image

def generate_app_icon_svg():
    svg = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">
  <defs>
    <!-- Background Gradient -->
    <linearGradient id="bgGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#070B16"/>
      <stop offset="35%" stop-color="#0B132B"/>
      <stop offset="70%" stop-color="#080E21"/>
      <stop offset="100%" stop-color="#04060D"/>
    </linearGradient>

    <!-- Outer Bevel Rim Gradient -->
    <linearGradient id="rimGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#00F5FF" stop-opacity="0.95"/>
      <stop offset="30%" stop-color="#38BDF8" stop-opacity="0.5"/>
      <stop offset="70%" stop-color="#1E3A8A" stop-opacity="0.25"/>
      <stop offset="100%" stop-color="#00F5FF" stop-opacity="0.85"/>
    </linearGradient>

    <!-- Shield Face Metallic Gradients -->
    <linearGradient id="shieldLeft" x1="0%" y1="20%" x2="100%" y2="80%">
      <stop offset="0%" stop-color="#1E293B"/>
      <stop offset="50%" stop-color="#0F172A"/>
      <stop offset="100%" stop-color="#080D1A"/>
    </linearGradient>

    <linearGradient id="shieldRight" x1="0%" y1="20%" x2="100%" y2="80%">
      <stop offset="0%" stop-color="#334155"/>
      <stop offset="40%" stop-color="#1E293B"/>
      <stop offset="100%" stop-color="#0F172A"/>
    </linearGradient>

    <!-- Shield Neon Contour -->
    <linearGradient id="shieldBorder" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#00FFFF"/>
      <stop offset="50%" stop-color="#0284C7"/>
      <stop offset="100%" stop-color="#38BDF8"/>
    </linearGradient>

    <!-- Inner Shield Depth Plate -->
    <linearGradient id="shieldInnerPlate" x1="0%" y1="0%" x2="0%" y2="100%">
      <stop offset="0%" stop-color="#0B1226"/>
      <stop offset="50%" stop-color="#070C1A"/>
      <stop offset="100%" stop-color="#03060E"/>
    </linearGradient>

    <!-- Laser Blade Slash Gradient -->
    <linearGradient id="laserGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#FF1E56"/>
      <stop offset="25%" stop-color="#FF0055"/>
      <stop offset="65%" stop-color="#FF5722"/>
      <stop offset="100%" stop-color="#FFB300"/>
    </linearGradient>

    <linearGradient id="laserCore" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#FFFFFF"/>
      <stop offset="50%" stop-color="#FFE4E6"/>
      <stop offset="100%" stop-color="#FFD166"/>
    </linearGradient>

    <!-- ADS Glyph Intact Metallic (Bottom-Left) -->
    <linearGradient id="adsIntactGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#FFFFFF"/>
      <stop offset="30%" stop-color="#E2E8F0"/>
      <stop offset="70%" stop-color="#94A3B8"/>
      <stop offset="100%" stop-color="#64748B"/>
    </linearGradient>

    <!-- ADS Glyph Burning Fractured (Top-Right) -->
    <linearGradient id="adsBurningGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#FF8C00"/>
      <stop offset="35%" stop-color="#EF4444"/>
      <stop offset="80%" stop-color="#DC2626"/>
      <stop offset="100%" stop-color="#991B1B"/>
    </linearGradient>

    <!-- Ambient Radial Glows -->
    <radialGradient id="centerGlow" cx="50%" cy="48%" r="50%">
      <stop offset="0%" stop-color="#00F5FF" stop-opacity="0.32"/>
      <stop offset="55%" stop-color="#0284C7" stop-opacity="0.12"/>
      <stop offset="100%" stop-color="#000000" stop-opacity="0"/>
    </radialGradient>

    <radialGradient id="slashCenterBurst" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="#FFFFFF" stop-opacity="1.0"/>
      <stop offset="20%" stop-color="#FFD166" stop-opacity="0.9"/>
      <stop offset="45%" stop-color="#FF0055" stop-opacity="0.75"/>
      <stop offset="75%" stop-color="#FF3366" stop-opacity="0.25"/>
      <stop offset="100%" stop-color="#FF0055" stop-opacity="0"/>
    </radialGradient>

    <!-- Glow & Shadow Filters -->
    <filter id="neonCyanGlow" x="-25%" y="-25%" width="150%" height="150%">
      <feGaussianBlur stdDeviation="7" result="blur"/>
      <feMerge>
        <feMergeNode in="blur"/>
        <feMergeNode in="SourceGraphic"/>
      </feMerge>
    </filter>

    <filter id="laserBladeGlow" x="-40%" y="-40%" width="180%" height="180%">
      <feGaussianBlur stdDeviation="6" result="b1"/>
      <feGaussianBlur stdDeviation="14" result="b2"/>
      <feMerge>
        <feMergeNode in="b2"/>
        <feMergeNode in="b1"/>
        <feMergeNode in="SourceGraphic"/>
      </feMerge>
    </filter>

    <filter id="shieldShadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="14" stdDeviation="16" flood-color="#000000" flood-opacity="0.8"/>
    </filter>

    <!-- Clip Paths -->
    <clipPath id="squircleClip">
      <rect x="18" y="18" width="476" height="476" rx="104" ry="104"/>
    </clipPath>

    <!-- Cut Line: (420, 110) to (92, 385) -->
    <!-- Top-Right Half Clip (Fractured) -->
    <clipPath id="cutTopRightClip">
      <polygon points="92,385 420,110 512,0 512,512 300,512 92,385"/>
    </clipPath>

    <!-- Bottom-Left Half Clip (Intact) -->
    <clipPath id="cutBottomLeftClip">
      <polygon points="92,385 420,110 0,0 0,512 92,512"/>
    </clipPath>
  </defs>

  <!-- 1. BASE CONTAINER: Obsidian Cyber Squircle -->
  <g clip-path="url(#squircleClip)">
    <rect x="18" y="18" width="476" height="476" rx="104" ry="104" fill="url(#bgGrad)"/>

    <!-- Subtle Tech Circuit Grid -->
    <g stroke="#00E5FF" stroke-opacity="0.08" stroke-width="1.2" fill="none">
      <path d="M 40 100 L 120 100 L 160 140 L 160 220"/>
      <path d="M 472 100 L 392 100 L 352 140 L 352 220"/>
      <path d="M 40 412 L 120 412 L 160 372 L 160 300"/>
      <path d="M 472 412 L 392 412 L 352 372 L 352 300"/>
      <circle cx="120" cy="100" r="3" fill="#00E5FF" fill-opacity="0.25"/>
      <circle cx="392" cy="100" r="3" fill="#00E5FF" fill-opacity="0.25"/>
      <circle cx="120" cy="412" r="3" fill="#00E5FF" fill-opacity="0.25"/>
      <circle cx="392" cy="412" r="3" fill="#00E5FF" fill-opacity="0.25"/>
    </g>

    <!-- Radial Cyan Aura Behind Shield -->
    <circle cx="256" cy="245" r="215" fill="url(#centerGlow)"/>

    <!-- Top Gloss Sheen -->
    <path d="M 24 24 Q 256 100 488 24 L 488 150 Q 256 110 24 150 Z" fill="#FFFFFF" fill-opacity="0.035"/>
  </g>

  <!-- Outer Luminous Rim Bevel -->
  <rect x="18" y="18" width="476" height="476" rx="104" ry="104"
        fill="none" stroke="url(#rimGrad)" stroke-width="4.5"/>
  <rect x="22" y="22" width="468" height="468" rx="100" ry="100"
        fill="none" stroke="#FFFFFF" stroke-opacity="0.14" stroke-width="1.2"/>

  <!-- 2. THE CYBER SHIELD (HERO AEGIS) -->
  <g filter="url(#shieldShadow)">
    <!-- Outer Cyan Neon Silhouette Aura -->
    <path d="M 256 52
             L 404 116
             C 404 275 346 375 256 434
             C 166 375 108 275 108 116 Z"
          fill="none" stroke="#00F5FF" stroke-opacity="0.45" stroke-width="14"
          filter="url(#neonCyanGlow)"/>

    <!-- Left Metallic Plate (Shadow facet) -->
    <path d="M 256 56
             L 114 118
             C 114 270 170 368 256 426 Z"
          fill="url(#shieldLeft)"/>

    <!-- Right Metallic Plate (Light facet) -->
    <path d="M 256 56
             L 398 118
             C 398 270 342 368 256 426 Z"
          fill="url(#shieldRight)"/>

    <!-- Central Ridge Highlight -->
    <path d="M 256 56 L 256 426" stroke="#38BDF8" stroke-opacity="0.4" stroke-width="2.5"/>

    <!-- Inner Neon Contour -->
    <path d="M 256 68
             L 384 124
             C 384 258 332 350 256 404
             C 180 350 128 258 128 124 Z"
          fill="none" stroke="url(#shieldBorder)" stroke-width="4.5"/>

    <!-- Inner Shield Cavity Plate -->
    <path d="M 256 80
             L 370 132
             C 370 248 322 334 256 384
             C 190 334 142 248 142 132 Z"
          fill="url(#shieldInnerPlate)"/>

    <!-- Subtle Tech Circuit Traces Inside Shield -->
    <g stroke="#00E5FF" stroke-opacity="0.15" stroke-width="1.8" fill="none">
      <path d="M 160 160 L 256 205 L 352 160"/>
      <path d="M 172 224 L 256 264 L 340 224"/>
      <path d="M 194 300 L 256 332 L 318 300"/>
    </g>
  </g>

  <!-- 3. THE "ADS" GLYPH (CLEAVED IN HALF) -->
  <!-- ADS Path Definition (A, D, S side by side) -->
  <!-- Letter A: [154..226] | Letter D: [236..308] | Letter S: [318..378] -->

  <!-- (A) Bottom-Left Intact Segment (Cool Titanium/Silver) -->
  <g clip-path="url(#cutBottomLeftClip)">
    <!-- Letter A -->
    <path d="M 190 190 L 150 304 L 176 304 L 184 278 L 208 278 L 216 304 L 242 304 L 202 190 Z
             M 196 230 L 203 256 L 189 256 Z"
          fill="url(#adsIntactGrad)" stroke="#38BDF8" stroke-width="2"/>

    <!-- Letter D -->
    <path d="M 240 190 L 284 190 C 308 190 322 205 322 247 C 322 289 308 304 284 304 L 240 304 Z
             M 264 214 L 264 280 L 282 280 C 294 280 300 270 300 247 C 300 224 294 214 282 214 Z"
          fill="url(#adsIntactGrad)" stroke="#38BDF8" stroke-width="2"/>

    <!-- Letter S -->
    <path d="M 334 220 C 334 198 348 190 368 190 C 388 190 398 200 398 216 L 374 216 C 374 208 370 206 366 206 C 360 206 356 209 356 216 C 356 234 400 236 400 268 C 400 292 384 304 366 304 C 344 304 332 292 332 272 L 356 272 C 356 282 361 286 368 286 C 374 286 378 282 378 274 C 378 254 334 252 334 220 Z"
          fill="url(#adsIntactGrad)" stroke="#38BDF8" stroke-width="2"/>
  </g>

  <!-- (B) Top-Right Fractured Segment (Displaced +10px X, -10px Y, Blazing Crimson/Amber) -->
  <g clip-path="url(#cutTopRightClip)" transform="translate(10, -10)">
    <!-- Letter A -->
    <path d="M 190 190 L 150 304 L 176 304 L 184 278 L 208 278 L 216 304 L 242 304 L 202 190 Z
             M 196 230 L 203 256 L 189 256 Z"
          fill="url(#adsBurningGrad)" stroke="#FF8C00" stroke-width="2"/>

    <!-- Letter D -->
    <path d="M 240 190 L 284 190 C 308 190 322 205 322 247 C 322 289 308 304 284 304 L 240 304 Z
             M 264 214 L 264 280 L 282 280 C 294 280 300 270 300 247 C 300 224 294 214 282 214 Z"
          fill="url(#adsBurningGrad)" stroke="#FF8C00" stroke-width="2"/>

    <!-- Letter S -->
    <path d="M 334 220 C 334 198 348 190 368 190 C 388 190 398 200 398 216 L 374 216 C 374 208 370 206 366 206 C 360 206 356 209 356 216 C 356 234 400 236 400 268 C 400 292 384 304 366 304 C 344 304 332 292 332 272 L 356 272 C 356 282 361 286 368 286 C 374 286 378 282 378 274 C 378 254 334 252 334 220 Z"
          fill="url(#adsBurningGrad)" stroke="#FF8C00" stroke-width="2"/>
  </g>

  <!-- Explosive Shards & Debris Particles Flying off Cut -->
  <g fill="#FF0055" stroke="#FFD166" stroke-width="1.2">
    <polygon points="310,182 322,176 326,188 314,194"/>
    <polygon points="338,162 348,158 350,168 340,172"/>
    <polygon points="274,155 284,152 286,162 276,165"/>
    <polygon points="360,192 370,186 374,196 364,202"/>
    <polygon points="218,298 226,294 228,304 220,308"/>
    <polygon points="174,332 182,328 184,338 176,342"/>
  </g>
  <circle cx="342" cy="180" r="3" fill="#FFFFFF"/>
  <circle cx="295" cy="168" r="2.5" fill="#FFB300"/>
  <circle cx="230" cy="285" r="2.5" fill="#FFFFFF"/>

  <!-- 4. THE EPIC LASER CLEAVER (KILLER ENERGY STRIKE) -->
  <g filter="url(#laserBladeGlow)">
    <!-- Broad Neon Red Aura Beam -->
    <line x1="428" y1="102" x2="84" y2="392"
          stroke="url(#laserGrad)" stroke-width="18" stroke-linecap="round" opacity="0.45"/>

    <!-- Sharp Laser Blade -->
    <line x1="422" y1="108" x2="90" y2="386"
          stroke="url(#laserGrad)" stroke-width="7.5" stroke-linecap="round"/>

    <!-- Hyper-White Core Beam -->
    <line x1="416" y1="114" x2="96" y2="380"
          stroke="url(#laserCore)" stroke-width="3" stroke-linecap="round"/>

    <!-- Tip Spark Flares -->
    <circle cx="420" cy="110" r="5" fill="#FFFFFF"/>
    <circle cx="92" cy="384" r="5" fill="#FFFFFF"/>
  </g>

  <!-- 5. CENTRAL BLAST IMPACT STARBURST -->
  <circle cx="256" cy="247" r="48" fill="url(#slashCenterBurst)"/>
  <circle cx="256" cy="247" r="7" fill="#FFFFFF"/>

  <!-- Starburst Flare Rays -->
  <line x1="214" y1="247" x2="298" y2="247" stroke="#FFFFFF" stroke-width="2.5" stroke-linecap="round" opacity="0.9"/>
  <line x1="256" y1="205" x2="256" y2="289" stroke="#FFFFFF" stroke-width="2.5" stroke-linecap="round" opacity="0.9"/>
  <line x1="228" y1="219" x2="284" y2="275" stroke="#FF8C00" stroke-width="3" stroke-linecap="round" opacity="0.8"/>

  <!-- 6. TOP CYBER GEM APEX -->
  <g filter="url(#neonCyanGlow)">
    <polygon points="256,44 270,62 256,80 242,62" fill="#00F5FF"/>
    <polygon points="256,48 266,62 256,76 246,62" fill="#FFFFFF"/>
  </g>

  <!-- 7. BOTTOM WORDMARK BADGE: "ADSKILLER" -->
  <g filter="url(#neonCyanGlow)">
    <rect x="176" y="446" width="160" height="28" rx="6" fill="#0B132B" stroke="#00F5FF" stroke-width="1.6"/>
    <text x="256" y="465"
          text-anchor="middle"
          fill="#FFFFFF"
          font-family="'Segoe UI', -apple-system, Roboto, sans-serif"
          font-weight="900"
          font-size="13.5"
          letter-spacing="3.5">ADSKILLER</text>
  </g>
</svg>
'''
    return svg

def generate_update_icon_svg():
    svg = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">
  <defs>
    <!-- Background Gradient -->
    <linearGradient id="upBgGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#060A14"/>
      <stop offset="35%" stop-color="#0B132B"/>
      <stop offset="70%" stop-color="#080E21"/>
      <stop offset="100%" stop-color="#04060D"/>
    </linearGradient>

    <!-- Outer Bevel Rim Gradient -->
    <linearGradient id="upRimGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#00F5FF" stop-opacity="0.95"/>
      <stop offset="30%" stop-color="#10B981" stop-opacity="0.6"/>
      <stop offset="70%" stop-color="#1E3A8A" stop-opacity="0.25"/>
      <stop offset="100%" stop-color="#00F5FF" stop-opacity="0.85"/>
    </linearGradient>

    <!-- Orbital Arc 1: Cyan to Azure Gradient -->
    <linearGradient id="arcCyanGrad" x1="0%" y1="0%" x2="100%" y2="100%">
      <stop offset="0%" stop-color="#00F5FF"/>
      <stop offset="50%" stop-color="#0284C7"/>
      <stop offset="100%" stop-color="#3B82F6"/>
    </linearGradient>

    <!-- Orbital Arc 2: Emerald to Cyan Gradient -->
    <linearGradient id="arcEmeraldGrad" x1="0%" y1="100%" x2="100%" y2="0%">
      <stop offset="0%" stop-color="#10B981"/>
      <stop offset="50%" stop-color="#06B6D4"/>
      <stop offset="100%" stop-color="#00F5FF"/>
    </linearGradient>

    <!-- Central Download Rocket Arrow 3D Gradient -->
    <linearGradient id="arrowMainGrad" x1="0%" y1="0%" x2="0%" y2="100%">
      <stop offset="0%" stop-color="#FFFFFF"/>
      <stop offset="35%" stop-color="#E0F2FE"/>
      <stop offset="70%" stop-color="#38BDF8"/>
      <stop offset="100%" stop-color="#0284C7"/>
    </linearGradient>

    <!-- Docking Platform Gradient -->
    <linearGradient id="dockGrad" x1="0%" y1="0%" x2="100%" y2="0%">
      <stop offset="0%" stop-color="#064E3B"/>
      <stop offset="50%" stop-color="#0284C7"/>
      <stop offset="100%" stop-color="#064E3B"/>
    </linearGradient>

    <!-- Ambient Center Glow -->
    <radialGradient id="upCenterGlow" cx="50%" cy="46%" r="50%">
      <stop offset="0%" stop-color="#00F5FF" stop-opacity="0.32"/>
      <stop offset="50%" stop-color="#10B981" stop-opacity="0.15"/>
      <stop offset="100%" stop-color="#000000" stop-opacity="0"/>
    </radialGradient>

    <radialGradient id="arrowImpactGlow" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="#FFFFFF" stop-opacity="0.9"/>
      <stop offset="30%" stop-color="#00F5FF" stop-opacity="0.75"/>
      <stop offset="70%" stop-color="#0284C7" stop-opacity="0.2"/>
      <stop offset="100%" stop-color="#000000" stop-opacity="0"/>
    </radialGradient>

    <!-- Glow & Shadow Filters -->
    <filter id="upNeonGlow" x="-25%" y="-25%" width="150%" height="150%">
      <feGaussianBlur stdDeviation="8" result="blur"/>
      <feMerge>
        <feMergeNode in="blur"/>
        <feMergeNode in="SourceGraphic"/>
      </feMerge>
    </filter>

    <filter id="upArrowShadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="12" stdDeviation="14" flood-color="#000000" flood-opacity="0.8"/>
    </filter>

    <!-- Clip Paths -->
    <clipPath id="upSquircleClip">
      <rect x="18" y="18" width="476" height="476" rx="104" ry="104"/>
    </clipPath>
  </defs>

  <!-- 1. BASE CONTAINER: Obsidian Squircle -->
  <g clip-path="url(#upSquircleClip)">
    <rect x="18" y="18" width="476" height="476" rx="104" ry="104" fill="url(#upBgGrad)"/>

    <!-- Tech Circuit Traces -->
    <g stroke="#00E5FF" stroke-opacity="0.08" stroke-width="1.2" fill="none">
      <path d="M 40 100 L 120 100 L 160 140 L 160 220"/>
      <path d="M 472 100 L 392 100 L 352 140 L 352 220"/>
      <path d="M 40 412 L 120 412 L 160 372 L 160 300"/>
      <path d="M 472 412 L 392 412 L 352 372 L 352 300"/>
      <circle cx="120" cy="100" r="3" fill="#00E5FF" fill-opacity="0.25"/>
      <circle cx="392" cy="100" r="3" fill="#00E5FF" fill-opacity="0.25"/>
      <circle cx="120" cy="412" r="3" fill="#10B981" fill-opacity="0.25"/>
      <circle cx="392" cy="412" r="3" fill="#10B981" fill-opacity="0.25"/>
    </g>

    <!-- Radiant Ambient Core Glow -->
    <circle cx="256" cy="235" r="215" fill="url(#upCenterGlow)"/>

    <!-- Orbital Dashed Guide Rings -->
    <circle cx="256" cy="235" r="162" fill="none" stroke="#00F5FF" stroke-opacity="0.12" stroke-width="1.5" stroke-dasharray="6,8"/>
    <circle cx="256" cy="235" r="128" fill="none" stroke="#10B981" stroke-opacity="0.10" stroke-width="1.2" stroke-dasharray="4,6"/>

    <!-- Top Gloss Sheen -->
    <path d="M 24 24 Q 256 100 488 24 L 488 150 Q 256 110 24 150 Z" fill="#FFFFFF" fill-opacity="0.035"/>
  </g>

  <!-- Outer Luminous Rim Bevel -->
  <rect x="18" y="18" width="476" height="476" rx="104" ry="104"
        fill="none" stroke="url(#upRimGrad)" stroke-width="4.5"/>
  <rect x="22" y="22" width="468" height="468" rx="100" ry="100"
        fill="none" stroke="#FFFFFF" stroke-opacity="0.14" stroke-width="1.2"/>

  <!-- 2. DUAL HIGH-TECH ORBITAL VORTEX LOOPS -->
  <g filter="url(#upNeonGlow)">
    <!-- Loop 1: Top-Right Sweeping Arc (Clockwise to bottom-right) -->
    <!-- Center (256, 235), Radius ~ 152 -->
    <path d="M 124 195
             A 152 152 0 0 1 398 185"
          fill="none" stroke="url(#arcCyanGrad)" stroke-width="16" stroke-linecap="round"/>

    <!-- Arrowhead for Loop 1 (at 398, 185 pointing down-right) -->
    <polygon points="380,158 424,196 384,216 394,188" fill="#00F5FF"/>

    <!-- Loop 2: Bottom-Left Sweeping Arc (Clockwise to top-left) -->
    <path d="M 388 275
             A 152 152 0 0 1 114 285"
          fill="none" stroke="url(#arcEmeraldGrad)" stroke-width="16" stroke-linecap="round"/>

    <!-- Arrowhead for Loop 2 (at 114, 285 pointing up-left) -->
    <polygon points="132,312 88,274 128,254 118,282" fill="#10B981"/>

    <!-- Speed Particle Dots Trailing in Orbit -->
    <circle cx="160" cy="142" r="3.5" fill="#00F5FF"/>
    <circle cx="205" cy="106" r="4.5" fill="#FFFFFF"/>
    <circle cx="352" cy="328" r="3.5" fill="#10B981"/>
    <circle cx="308" cy="364" r="4.5" fill="#FFFFFF"/>
  </g>

  <!-- 3. CENTRAL HIGH-SPEED FIRMWARE DOWNLOAD ROCKET ARROW -->
  <g filter="url(#upArrowShadow)">
    <!-- Level 1 Top Chevron (V) -->
    <path d="M 226 108 L 256 128 L 286 108 L 294 118 L 256 144 L 218 118 Z"
          fill="#38BDF8" opacity="0.6" filter="url(#upNeonGlow)"/>

    <!-- Level 2 Middle Chevron (V) -->
    <path d="M 216 142 L 256 168 L 296 142 L 306 154 L 256 188 L 206 154 Z"
          fill="#00F5FF" opacity="0.85" filter="url(#upNeonGlow)"/>

    <!-- Level 3 Main Arrow Shaft & Broad Arrowhead -->
    <!-- Outer Glow Silhouette -->
    <path d="M 238 186 L 274 186 L 274 246 L 316 246 L 256 316 L 196 246 L 238 246 Z"
          fill="none" stroke="#00F5FF" stroke-width="10" filter="url(#upNeonGlow)" opacity="0.5"/>

    <!-- Main Solid Arrow Body -->
    <path d="M 238 186 L 274 186 L 274 246 L 316 246 L 256 316 L 196 246 L 238 246 Z"
          fill="url(#arrowMainGrad)" stroke="#38BDF8" stroke-width="2.5"/>

    <!-- Specular Ridge Highlight Line down Center of Arrow -->
    <line x1="256" y1="188" x2="256" y2="306" stroke="#FFFFFF" stroke-width="2.8" stroke-linecap="round"/>
  </g>

  <!-- 4. DOCKING ENERGY BASE / CHIP RECEIVER -->
  <g filter="url(#upNeonGlow)">
    <!-- Receiver Dock Platform -->
    <rect x="180" y="342" width="152" height="12" rx="4" fill="url(#dockGrad)" stroke="#00F5FF" stroke-width="1.8"/>

    <!-- Energy Docking Pin Indicators -->
    <line x1="200" y1="358" x2="200" y2="368" stroke="#10B981" stroke-width="3" stroke-linecap="round"/>
    <line x1="228" y1="358" x2="228" y2="368" stroke="#00F5FF" stroke-width="3" stroke-linecap="round"/>
    <line x1="256" y1="358" x2="256" y2="372" stroke="#FFFFFF" stroke-width="3.5" stroke-linecap="round"/>
    <line x1="284" y1="358" x2="284" y2="368" stroke="#00F5FF" stroke-width="3" stroke-linecap="round"/>
    <line x1="312" y1="358" x2="312" y2="368" stroke="#10B981" stroke-width="3" stroke-linecap="round"/>

    <!-- Impact Energy Flare at Arrow Tip -->
    <circle cx="256" cy="318" r="32" fill="url(#arrowImpactGlow)"/>
    <circle cx="256" cy="318" r="4.5" fill="#FFFFFF"/>
    <line x1="236" y1="318" x2="276" y2="318" stroke="#FFFFFF" stroke-width="2" stroke-linecap="round"/>
  </g>

  <!-- 5. TOP SPEED PULSE ICON (FLASH SYMBOL) -->
  <g filter="url(#upNeonGlow)">
    <polygon points="256,52 268,68 256,84 244,68" fill="#10B981"/>
    <polygon points="256,56 264,68 256,80 248,68" fill="#FFFFFF"/>
  </g>

  <!-- 6. BOTTOM WORDMARK BADGE: "UPDATE" -->
  <g filter="url(#upNeonGlow)">
    <rect x="194" y="446" width="124" height="28" rx="6" fill="#0B132B" stroke="#00F5FF" stroke-width="1.6"/>
    <text x="256" y="465"
          text-anchor="middle"
          fill="#FFFFFF"
          font-family="'Segoe UI', -apple-system, Roboto, sans-serif"
          font-weight="900"
          font-size="13.5"
          letter-spacing="4">UPDATE</text>
  </g>
</svg>
'''
    return svg

def main():
    base_dir = "/media/dev/adskiller"
    res_dir = os.path.join(base_dir, "res")

    app_svg_path = os.path.join(res_dir, "icon.svg")
    update_svg_path = os.path.join(res_dir, "icon_update.svg")

    print(f"Writing {app_svg_path}...")
    with open(app_svg_path, "w", encoding="utf-8") as f:
        f.write(generate_app_icon_svg())

    print(f"Writing {update_svg_path}...")
    with open(update_svg_path, "w", encoding="utf-8") as f:
        f.write(generate_update_icon_svg())

    # Render PNGs using rsvg-convert
    print("Rendering PNGs with rsvg-convert...")
    
    # App icon: 256x256 and 512x512
    app_png_256 = os.path.join(res_dir, "icon_std.png")
    app_png_512 = os.path.join(res_dir, "icon.png")
    
    subprocess.run(["rsvg-convert", "-w", "256", "-h", "256", app_svg_path, "-o", app_png_256], check=True)
    subprocess.run(["rsvg-convert", "-w", "512", "-h", "512", app_svg_path, "-o", app_png_512], check=True)

    # Update icon: 256x256
    update_png_256 = os.path.join(res_dir, "icon_update.png")
    subprocess.run(["rsvg-convert", "-w", "256", "-h", "256", update_svg_path, "-o", update_png_256], check=True)

    # Convert to multi-size Windows ICO with PIL
    ico_sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]

    print("Generating Windows ICO files...")
    app_ico_path = os.path.join(res_dir, "icon.ico")
    with Image.open(app_png_512) as img:
        img.save(app_ico_path, format="ICO", sizes=ico_sizes)
    print(f"Saved {app_ico_path}")

    update_ico_path = os.path.join(res_dir, "icon_update.ico")
    with Image.open(update_png_256) as img:
        img.save(update_ico_path, format="ICO", sizes=ico_sizes)
    print(f"Saved {update_ico_path}")

    print("All icons successfully generated!")

if __name__ == "__main__":
    main()

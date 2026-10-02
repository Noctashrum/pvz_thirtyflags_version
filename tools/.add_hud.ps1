$ErrorActionPreference = "Stop"
. "D:\dsh-project\LawnProject\tools\gbkfunc.ps1"

$file = "D:\dsh-project\LawnProject\src\Lawn\ThirtyFlags.cpp"

$old = "            g->DrawImage(Sexy::IMAGE_POOL, TF_LAWN_LEFT - 30, aY - 5, Rect(0, 0, aPoolW, aPoolH));`r`n        }`r`n    }`r`n}`r`n"

$new = @'
            g->DrawImage(Sexy::IMAGE_POOL, TF_LAWN_LEFT - 30, aY - 5, Rect(0, 0, aPoolW, aPoolH));
        }
    }

    // ---- 4) 自检 HUD：把不可见的运行状态画在左上角，一眼即可验证解锁/水路是否按旗推进 ----
    {
        SexyString aLine1 = StrFormat(_S("FLAG %d/30"), gThirtyFlags.mFlag);
        SexyString aLine2 = StrFormat(_S("unlock %d%d%d%d%d"),
            gThirtyFlags.IsRowUnlocked(0) ? 1 : 0, gThirtyFlags.IsRowUnlocked(1) ? 1 : 0,
            gThirtyFlags.IsRowUnlocked(2) ? 1 : 0, gThirtyFlags.IsRowUnlocked(3) ? 1 : 0,
            gThirtyFlags.IsRowUnlocked(4) ? 1 : 0);
        SexyString aLine3 = StrFormat(_S("water %d%d%d%d%d"),
            gThirtyFlags.IsRowUnlocked(0) ? (gThirtyFlags.IsWaterRow(0) ? 1 : 0) : 9,
            gThirtyFlags.IsRowUnlocked(1) ? (gThirtyFlags.IsWaterRow(1) ? 1 : 0) : 9,
            gThirtyFlags.IsRowUnlocked(2) ? (gThirtyFlags.IsWaterRow(2) ? 1 : 0) : 9,
            gThirtyFlags.IsRowUnlocked(3) ? (gThirtyFlags.IsWaterRow(3) ? 1 : 0) : 9,
            gThirtyFlags.IsRowUnlocked(4) ? (gThirtyFlags.IsWaterRow(4) ? 1 : 0) : 9);
        SexyString aLine4 = StrFormat(_S("upg %d  elite %d%%  kill %d"),
            gThirtyFlags.mUpgradeCount, gThirtyFlags.GetEliteChancePercent(), gThirtyFlags.mTotalKills);

        g->SetFont(Sexy::FONT_DWARVENTODCRAFT12);
        g->SetColor(Color(0, 0, 0, 190));
        g->FillRect(2, 2, 152, 64);
        g->SetColor(Color(255, 255, 80));
        g->DrawString(aLine1, 8, 5);
        g->DrawString(aLine2, 8, 20);
        g->DrawString(aLine3, 8, 35);
        g->SetColor(Color(140, 255, 140));
        g->DrawString(aLine4, 8, 50);
    }
}
'@

Edit-Gbk $file $old $new

#include "ArenaShopWidget.h"

#include "ArenaUISounds.h"
#include "Async/Async.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Dom/JsonObject.h"
#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Rendering/DrawElements.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArenaShop"

namespace ArenaShopStyle
{
	// Fortnite shop palette
	const FLinearColor White(1.0f, 1.0f, 1.0f);
	const FLinearColor Ink(0.03f, 0.03f, 0.05f);
	const FLinearColor Muted(0.72f, 0.78f, 0.9f);
	const FLinearColor Dim(0.55f, 0.6f, 0.72f);
	const FLinearColor Yellow(1.0f, 0.82f, 0.02f);
	const FLinearColor YellowHover(1.0f, 0.9f, 0.3f);
	const FLinearColor NavyTop(0.004f, 0.03f, 0.14f);
	const FLinearColor BlueBottom(0.03f, 0.2f, 0.62f);
	const FLinearColor Glass(1.0f, 1.0f, 1.0f, 0.12f);
	const FLinearColor GlassHover(1.0f, 1.0f, 1.0f, 0.24f);

	constexpr float TileRadius = 12.0f;
	constexpr float TileGap = 14.0f;
	/** Height / width of a 1x1 tile (330 x 450 in Fortnite) */
	constexpr float TileAspect = 1.364f;
	constexpr float ContentMaxWidth = 1460.0f;
	constexpr float SideMargin = 60.0f;
	constexpr float HoverSeconds = 0.16f;
	/** Fortnite headings are a slanted Burbank */
	const FVector2f Italic(-9.0f, 0.0f);

	FLinearColor FromHex(const FString& Hex, const FLinearColor& Fallback)
	{
		if (Hex.Len() < 6)
		{
			return Fallback;
		}
		const FColor Color = FColor::FromHex(Hex);
		return FLinearColor::FromSRGBColor(FColor(Color.R, Color.G, Color.B, 255));
	}

	FLinearColor RarityColor(const FString& Rarity)
	{
		static const TMap<FString, FLinearColor> Colors = {
			{ TEXT("common"), FLinearColor(0.38f, 0.40f, 0.42f) },
			{ TEXT("uncommon"), FLinearColor(0.10f, 0.50f, 0.12f) },
			{ TEXT("rare"), FLinearColor(0.08f, 0.38f, 0.90f) },
			{ TEXT("epic"), FLinearColor(0.55f, 0.18f, 0.85f) },
			{ TEXT("legendary"), FLinearColor(0.92f, 0.45f, 0.08f) },
			{ TEXT("mythic"), FLinearColor(0.90f, 0.72f, 0.12f) },
			{ TEXT("icon"), FLinearColor(0.12f, 0.68f, 0.78f) },
			{ TEXT("marvel"), FLinearColor(0.78f, 0.10f, 0.12f) },
			{ TEXT("dc"), FLinearColor(0.18f, 0.28f, 0.55f) },
			{ TEXT("dark"), FLinearColor(0.62f, 0.08f, 0.55f) },
			{ TEXT("gaminglegends"), FLinearColor(0.28f, 0.14f, 0.70f) },
			{ TEXT("starwars"), FLinearColor(0.12f, 0.16f, 0.32f) },
			{ TEXT("frozen"), FLinearColor(0.35f, 0.70f, 0.90f) },
			{ TEXT("lava"), FLinearColor(0.85f, 0.30f, 0.05f) },
			{ TEXT("shadow"), FLinearColor(0.10f, 0.10f, 0.12f) },
			{ TEXT("slurp"), FLinearColor(0.10f, 0.72f, 0.70f) },
		};
		if (const FLinearColor* Color = Colors.Find(Rarity.ToLower()))
		{
			return *Color;
		}
		return FLinearColor(0.10f, 0.12f, 0.16f);
	}

	const FSlateBrush* StrikeBrush()
	{
		static FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	const FSlateBrush* WhiteBrush()
	{
		static FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Slate widgets
// ─────────────────────────────────────────────────────────────────────────────

/** Vertical two-colour gradient with rounded corners, drawn directly (no texture needed) */
class SArenaGradient : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SArenaGradient)
		: _Top(FLinearColor::Black), _Bottom(FLinearColor::Black), _Radius(0.0f) {}
		SLATE_ATTRIBUTE(FLinearColor, Top)
		SLATE_ATTRIBUTE(FLinearColor, Bottom)
		SLATE_ARGUMENT(float, Radius)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Top = InArgs._Top;
		Bottom = InArgs._Bottom;
		Radius = InArgs._Radius;
		SetVisibility(EVisibility::HitTestInvisible);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const float Opacity = InWidgetStyle.GetColorAndOpacityTint().A;
		FLinearColor TopColor = Top.Get();
		FLinearColor BottomColor = Bottom.Get();
		TopColor.A *= Opacity;
		BottomColor.A *= Opacity;
		TArray<FSlateGradientStop> Stops;
		Stops.Add(FSlateGradientStop(FVector2D::ZeroVector, TopColor));
		Stops.Add(FSlateGradientStop(FVector2D(AllottedGeometry.GetLocalSize()), BottomColor));
		FSlateDrawElement::MakeGradient(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Stops, Orient_Vertical,
			ESlateDrawEffect::None, FVector4f(Radius, Radius, Radius, Radius));
		return LayerId + 1;
	}

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.0f, 1.0f); }

private:
	TAttribute<FLinearColor> Top;
	TAttribute<FLinearColor> Bottom;
	float Radius = 0.0f;
};

/** Rounded pill button with the lobby's hover lift (ATRÁS, COMPRAR, REINTENTAR) */
class SArenaShopButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArenaShopButton)
		: _Fill(ArenaShopStyle::Glass), _FillHover(ArenaShopStyle::GlassHover), _TextColor(FLinearColor::White) {}
		SLATE_ARGUMENT(FText, Text)
		SLATE_ARGUMENT(FText, Hint)
		SLATE_ARGUMENT(FSlateFontInfo, Font)
		SLATE_ARGUMENT(FSlateFontInfo, HintFont)
		SLATE_ARGUMENT(FLinearColor, Fill)
		SLATE_ARGUMENT(FLinearColor, FillHover)
		SLATE_ARGUMENT(FLinearColor, TextColor)
		SLATE_EVENT(FSimpleDelegate, OnClicked)
		SLATE_EVENT(FSimpleDelegate, OnHovered)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Fill = InArgs._Fill;
		FillHover = InArgs._FillHover;
		OnClicked = InArgs._OnClicked;
		OnHovered = InArgs._OnHovered;
		HoverSequence.AddCurve(0.0f, ArenaShopStyle::HoverSeconds, ECurveEaseFunction::QuadOut);
		Brush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, 10.0f);
		SetCursor(EMouseCursor::Hand);

		TSharedRef<SVerticalBox> Labels = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(InArgs._Text)
				.Font(InArgs._Font)
				.ColorAndOpacity(InArgs._TextColor)
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			];
		if (!InArgs._Hint.IsEmpty())
		{
			Labels->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, -2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(InArgs._Hint)
				.Font(InArgs._HintFont)
				.ColorAndOpacity(FLinearColor(InArgs._TextColor.R, InArgs._TextColor.G, InArgs._TextColor.B, 0.7f))
				.TransformPolicy(ETextTransformPolicy::ToUpper)
			];
		}

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(Brush.Get())
			.BorderBackgroundColor(this, &SArenaShopButton::GetFillColor)
			.Padding(FMargin(28.0f, 9.0f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				Labels
			]
		];
		SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
		HoverSequence.Play(AsShared());
		OnHovered.ExecuteIfBound();
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		SCompoundWidget::OnMouseLeave(MouseEvent);
		HoverSequence.PlayReverse(AsShared());
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? FReply::Handled() : FReply::Unhandled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			OnClicked.ExecuteIfBound();
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		const float Hover = HoverSequence.GetLerp();
		SetRenderTransform(FSlateRenderTransform(FScale2D(1.0f + 0.04f * Hover)));
	}

private:
	FSlateColor GetFillColor() const { return FMath::Lerp(Fill, FillHover, HoverSequence.GetLerp()); }

	FLinearColor Fill;
	FLinearColor FillHover;
	FSimpleDelegate OnClicked;
	FSimpleDelegate OnHovered;
	FCurveSequence HoverSequence;
	TSharedPtr<FSlateRoundedBoxBrush> Brush;
};

/**
 * One offer of the shop. Hover (like Fortnite): the tile lifts and grows a little, a white rim lights up,
 * the art zooms in and the whole tile brightens; click opens the item detail.
 */
class SArenaShopTile : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArenaShopTile) {}
		SLATE_ARGUMENT(FArenaShopEntry, Entry)
		SLATE_ARGUMENT(TWeakObjectPtr<UArenaShopWidget>, Owner)
		SLATE_ATTRIBUTE(float, UnitSize)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		using namespace ArenaShopStyle;
		Entry = InArgs._Entry;
		Owner = InArgs._Owner;
		UnitSize = InArgs._UnitSize;
		HoverSequence.AddCurve(0.0f, HoverSeconds, ECurveEaseFunction::QuadOut);
		SetCursor(EMouseCursor::Hand);
		SetCanTick(true);

		UArenaShopWidget* Shop = Owner.Get();
		const FSlateFontInfo NameFont = Shop ? Shop->BoldFont(15) : FCoreStyle::GetDefaultFontStyle("Bold", 15);
		const FSlateFontInfo PriceFont = Shop ? Shop->BoldFont(14) : FCoreStyle::GetDefaultFontStyle("Bold", 14);
		const FSlateFontInfo OldPriceFont = Shop ? Shop->BodyFont(13) : FCoreStyle::GetDefaultFontStyle("Regular", 13);
		const FSlateFontInfo BannerFont = Shop ? Shop->BoldFont(10) : FCoreStyle::GetDefaultFontStyle("Bold", 10);
		const FSlateFontInfo PlusFont = Shop ? Shop->BoldFont(22) : FCoreStyle::GetDefaultFontStyle("Bold", 22);

		// Art brush: rounded so the corners of the tile stay clean, the UV region crops it like "cover" and zooms on hover
		ArtBrush = MakeShared<FSlateBrush>();
		ArtBrush->DrawAs = Entry.bImageIsTileArt ? ESlateBrushDrawType::RoundedBox : ESlateBrushDrawType::Image;
		ArtBrush->OutlineSettings = FSlateBrushOutlineSettings(FVector4(TileRadius, TileRadius, TileRadius, TileRadius));
		ArtBrush->OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		ArtBrush->TintColor = FLinearColor::White;
		LiftBrush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, TileRadius);
		RimBrush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::Transparent, TileRadius, FLinearColor::White, 2.5f);
		PillBrush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, 6.0f);

		// Base colours of the offer (the API gives them for every tile) with a darker foot for the text
		const FLinearColor Top = Entry.Color1;
		const FLinearColor Bottom = Entry.Color3 * 0.6f;

		TSharedRef<SOverlay> Layers = SNew(SOverlay);

		Layers->AddSlot()
		[
			SNew(SArenaGradient).Top(Top).Bottom(FLinearColor(Bottom.R, Bottom.G, Bottom.B, 1.0f)).Radius(TileRadius)
		];

		if (Entry.bImageIsTileArt)
		{
			Layers->AddSlot()
			[
				SAssignNew(ArtImage, SImage).Image(ArtBrush.Get()).Visibility(EVisibility::Collapsed)
			];
		}
		else
		{
			// Icons (no tile render): centred over the colours, zoomed with a transform instead of the UV crop
			Layers->AddSlot().Padding(FMargin(10.0f, 10.0f, 10.0f, 56.0f))
			[
				SAssignNew(ArtScaleBox, SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				.RenderTransformPivot(FVector2D(0.5f, 0.5f))
				.Visibility(EVisibility::Collapsed)
				[
					SAssignNew(ArtImage, SImage).Image(ArtBrush.Get())
				]
			];
		}

		// Foot shade so the name and price read over any art
		Layers->AddSlot().VAlign(VAlign_Bottom)
		[
			SNew(SBox).HeightOverride(this, &SArenaShopTile::GetShadeHeight)
			[
				SNew(SArenaGradient)
				.Top(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
				.Bottom(FLinearColor(0.0f, 0.0f, 0.0f, 0.86f))
				.Radius(TileRadius)
			]
		];

		// Brightening on hover
		Layers->AddSlot()
		[
			SNew(SBorder)
			.BorderImage(LiftBrush.Get())
			.BorderBackgroundColor(this, &SArenaShopTile::GetLiftColor)
			.Visibility(EVisibility::HitTestInvisible)
		];

		// Name, banner and price
		TSharedRef<SVerticalBox> Labels = SNew(SVerticalBox);
		if (!Entry.BannerText.IsEmpty())
		{
			Labels->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0.0f, 0.0f, 0.0f, 5.0f)
			[
				SNew(SBorder)
				.BorderImage(PillBrush.Get())
				.BorderBackgroundColor(FLinearColor::White)
				.Padding(FMargin(7.0f, 2.0f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(Entry.BannerText))
					.Font(BannerFont)
					.ColorAndOpacity(Ink)
					.TransformPolicy(ETextTransformPolicy::ToUpper)
					.RenderTransform(FSlateRenderTransform(FShear2D::FromShearAngles(Italic)))
				]
			];
		}
		Labels->AddSlot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Entry.Title))
			.Font(NameFont)
			.ColorAndOpacity(White)
			.ShadowOffset(FVector2D(0.0f, 1.5f))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		];
		TSharedRef<SHorizontalBox> PriceRow = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(16.0f).HeightOverride(16.0f)
				[
					SNew(SImage).Image(Shop ? Shop->GetCurrencyBrush() : nullptr)
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(UArenaShopWidget::FormatPrice(Entry.FinalPrice)))
				.Font(PriceFont)
				.ColorAndOpacity(White)
			];
		if (Entry.RegularPrice > Entry.FinalPrice)
		{
			PriceRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(UArenaShopWidget::FormatPrice(Entry.RegularPrice)))
				.Font(OldPriceFont)
				.ColorAndOpacity(FLinearColor(0.75f, 0.78f, 0.85f, 0.9f))
				.StrikeBrush(StrikeBrush())
			];
		}
		Labels->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)[PriceRow];

		Layers->AddSlot().VAlign(VAlign_Bottom).HAlign(HAlign_Fill).Padding(FMargin(12.0f, 0.0f, 40.0f, 10.0f))
		[
			Labels
		];

		// "+" wishlist corner, like the web shop
		Layers->AddSlot().VAlign(VAlign_Bottom).HAlign(HAlign_Right).Padding(FMargin(0.0f, 0.0f, 12.0f, 6.0f))
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("+")))
			.Font(PlusFont)
			.ColorAndOpacity(White)
		];

		// White rim that lights up on hover
		Layers->AddSlot()
		[
			SAssignNew(Rim, SBorder)
			.BorderImage(RimBrush.Get())
			.Visibility(EVisibility::HitTestInvisible)
			.RenderOpacity(0.0f)
		];

		ChildSlot
		[
			SNew(SBox)
			.WidthOverride(this, &SArenaShopTile::GetWidth)
			.HeightOverride(this, &SArenaShopTile::GetHeight)
			.Clipping(EWidgetClipping::ClipToBounds)
			[
				Layers
			]
		];
		SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		// The art is only asked for once the tile has been drawn: scrolling through 300 offers must not download them all
		if (!bImageRequested && !Entry.ImageUrl.IsEmpty())
		{
			bImageRequested = true;
			if (UArenaShopWidget* Shop = Owner.Get())
			{
				TWeakPtr<SArenaShopTile> WeakThis = SharedThis(this);
				Shop->RequestImage(Entry.ImageUrl, WeakThis, [WeakThis](UTexture2D* Texture)
				{
					if (TSharedPtr<SArenaShopTile> Tile = WeakThis.Pin())
					{
						Tile->SetTexture(Texture);
					}
				});
			}
		}

		const float Hover = HoverSequence.GetLerp();
		SetRenderTransform(FSlateRenderTransform(FScale2D(1.0f + 0.035f * Hover)));
		if (Rim.IsValid())
		{
			Rim->SetRenderOpacity(Hover);
		}
		if (Entry.bImageIsTileArt)
		{
			UpdateArtCrop(AllottedGeometry.GetLocalSize(), 1.0f + 0.08f * Hover);
		}
		else if (ArtScaleBox.IsValid())
		{
			ArtScaleBox->SetRenderTransform(FSlateRenderTransform(FScale2D(1.0f + 0.07f * Hover), FVector2f(0.0f, -6.0f * Hover)));
		}
	}

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
		HoverSequence.Play(AsShared());
		if (UArenaShopWidget* Shop = Owner.Get())
		{
			Shop->PlayHoverSound();
		}
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		SCompoundWidget::OnMouseLeave(MouseEvent);
		HoverSequence.PlayReverse(AsShared());
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton ? FReply::Handled() : FReply::Unhandled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
		{
			if (UArenaShopWidget* Shop = Owner.Get())
			{
				Shop->ShowDetail(Entry);
			}
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	void SetTexture(UTexture2D* Texture)
	{
		if (Texture == nullptr || !ArtImage.IsValid())
		{
			return;
		}
		TextureSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		ArtBrush->SetResourceObject(Texture);
		ArtBrush->ImageSize = TextureSize;
		ArtImage->SetVisibility(EVisibility::HitTestInvisible);
		if (ArtScaleBox.IsValid())
		{
			ArtScaleBox->SetVisibility(EVisibility::HitTestInvisible);
		}
	}

private:
	FOptionalSize GetWidth() const
	{
		const float Unit = UnitSize.Get(300.0f);
		return Entry.Columns * Unit + (Entry.Columns - 1) * ArenaShopStyle::TileGap;
	}

	FOptionalSize GetHeight() const
	{
		const float UnitHeight = UnitSize.Get(300.0f) * ArenaShopStyle::TileAspect;
		return Entry.Rows * UnitHeight + (Entry.Rows - 1) * ArenaShopStyle::TileGap;
	}

	FOptionalSize GetShadeHeight() const { return GetHeight().Get() * 0.42f; }

	FSlateColor GetLiftColor() const { return FLinearColor(1.0f, 1.0f, 1.0f, 0.07f * HoverSequence.GetLerp()); }

	/** Crops the texture to the tile aspect ("cover") and zooms towards the centre */
	void UpdateArtCrop(const FVector2D& TileSize, float Zoom)
	{
		if (TextureSize.X <= 0.0f || TextureSize.Y <= 0.0f || TileSize.X <= 0.0f || TileSize.Y <= 0.0f)
		{
			return;
		}
		const float TextureAspect = TextureSize.X / TextureSize.Y;
		const float TileAspect = TileSize.X / TileSize.Y;
		float Width = 1.0f;
		float Height = 1.0f;
		if (TextureAspect > TileAspect)
		{
			Width = TileAspect / TextureAspect;
		}
		else
		{
			Height = TextureAspect / TileAspect;
		}
		Width /= Zoom;
		Height /= Zoom;
		// FSlateBrush::UVRegion is FBox2f in UE5 (FBox2D before): build it from its own types
		using FUVBox = decltype(FSlateBrush::UVRegion);
		using FUVVector = decltype(FUVBox::Min);
		ArtBrush->UVRegion = FUVBox(FUVVector(0.5f - Width * 0.5f, 0.5f - Height * 0.5f), FUVVector(0.5f + Width * 0.5f, 0.5f + Height * 0.5f));
	}

	FArenaShopEntry Entry;
	TWeakObjectPtr<UArenaShopWidget> Owner;
	TAttribute<float> UnitSize;
	FCurveSequence HoverSequence;
	TSharedPtr<FSlateBrush> ArtBrush;
	TSharedPtr<FSlateRoundedBoxBrush> LiftBrush;
	TSharedPtr<FSlateRoundedBoxBrush> RimBrush;
	TSharedPtr<FSlateRoundedBoxBrush> PillBrush;
	TSharedPtr<SImage> ArtImage;
	TSharedPtr<SScaleBox> ArtScaleBox;
	TSharedPtr<SBorder> Rim;
	FVector2D TextureSize = FVector2D::ZeroVector;
	bool bImageRequested = false;
};

/** Full screen item page: big art on the offer colours, name, rarity, description, set, price and COMPRAR */
class SArenaShopDetail : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArenaShopDetail) {}
		SLATE_ARGUMENT(FArenaShopEntry, Entry)
		SLATE_ARGUMENT(TWeakObjectPtr<UArenaShopWidget>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		using namespace ArenaShopStyle;
		Entry = InArgs._Entry;
		Owner = InArgs._Owner;
		UArenaShopWidget* Shop = Owner.Get();
		if (Shop == nullptr)
		{
			return;
		}

		ArtBrush = MakeShared<FSlateBrush>();
		ArtBrush->DrawAs = ESlateBrushDrawType::Image;
		PillBrush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, 6.0f);
		const FArenaShopItem* First = Entry.Items.IsValidIndex(0) ? &Entry.Items[0] : nullptr;
		const FLinearColor Accent = First ? RarityColor(First->RarityValue) : Entry.Color1;

		TSharedRef<SVerticalBox> Info = SNew(SVerticalBox);
		auto AddLine = [&Info, Shop](const FString& Text, int32 Size, const FLinearColor& Color, bool bBold, float Top = 0.0f, bool bUpper = false)
		{
			if (Text.IsEmpty())
			{
				return;
			}
			Info->AddSlot().AutoHeight().Padding(0.0f, Top, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Text))
				.Font(bBold ? Shop->BoldFont(Size) : Shop->BodyFont(Size))
				.ColorAndOpacity(Color)
				.AutoWrapText(true)
				.TransformPolicy(bUpper ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None)
				.ShadowOffset(FVector2D(0.0f, 1.5f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f))
			];
		};

		FString Kind = Entry.bBundle ? LOCTEXT("Bundle", "LOTE").ToString() : (First ? First->TypeName : FString());
		if (First && !First->SeriesName.IsEmpty())
		{
			Kind += TEXT(" · ") + First->SeriesName;
		}
		else if (First && !First->RarityName.IsEmpty())
		{
			Kind += TEXT(" · ") + First->RarityName;
		}
		AddLine(Kind, 14, Muted, true, 0.0f, true);
		Info->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Entry.Title))
			.Font(Shop->HeadingFont(40))
			.ColorAndOpacity(White)
			.AutoWrapText(true)
			.TransformPolicy(ETextTransformPolicy::ToUpper)
			.RenderTransform(FSlateRenderTransform(FShear2D::FromShearAngles(Italic)))
			.ShadowOffset(FVector2D(0.0f, 2.0f))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
		];
		if (First)
		{
			AddLine(First->Description, 17, White, false, 14.0f);
			AddLine(First->SetText, 15, Muted, false, 10.0f);
			AddLine(First->IntroductionText, 15, Muted, false, 2.0f);
		}
		if (Entry.Items.Num() > 1)
		{
			AddLine(FText::Format(LOCTEXT("BundleContents", "Incluye {0} objetos:"), Entry.Items.Num()).ToString(), 15, Muted, true, 16.0f);
			for (const FArenaShopItem& Item : Entry.Items)
			{
				AddLine(TEXT("• ") + Item.Name + (Item.TypeName.IsEmpty() ? FString() : TEXT("  (") + Item.TypeName + TEXT(")")), 15, White, false, 3.0f);
			}
		}

		TSharedRef<SHorizontalBox> PriceRow = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(26.0f).HeightOverride(26.0f)[SNew(SImage).Image(Shop->GetCurrencyBrush())]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(UArenaShopWidget::FormatPrice(Entry.FinalPrice))).Font(Shop->BoldFont(26)).ColorAndOpacity(White)
			];
		if (Entry.RegularPrice > Entry.FinalPrice)
		{
			PriceRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock).Text(FText::FromString(UArenaShopWidget::FormatPrice(Entry.RegularPrice))).Font(Shop->BodyFont(20))
				.ColorAndOpacity(FLinearColor(0.75f, 0.78f, 0.85f)).StrikeBrush(StrikeBrush())
			];
			if (!Entry.BannerText.IsEmpty())
			{
				PriceRow->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(14.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBorder).BorderImage(PillBrush.Get()).Padding(FMargin(8.0f, 3.0f))
					[
						SNew(STextBlock).Text(FText::FromString(Entry.BannerText)).Font(Shop->BoldFont(11)).ColorAndOpacity(Ink).TransformPolicy(ETextTransformPolicy::ToUpper)
					]
				];
			}
		}
		Info->AddSlot().AutoHeight().Padding(0.0f, 26.0f, 0.0f, 0.0f)[PriceRow];

		Info->AddSlot().AutoHeight().Padding(0.0f, 22.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SArenaShopButton)
				.Text(LOCTEXT("Buy", "Comprar"))
				.Font(Shop->BoldFont(18))
				.HintFont(Shop->BodyFont(9))
				.Fill(Yellow).FillHover(YellowHover).TextColor(Ink)
				.OnClicked(FSimpleDelegate::CreateSP(this, &SArenaShopDetail::HandleBuy))
				.OnHovered(FSimpleDelegate::CreateSP(this, &SArenaShopDetail::HandleHover))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(12.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SArenaShopButton)
				.Text(LOCTEXT("Back", "Atrás"))
				.Hint(LOCTEXT("EscHint", "Esc"))
				.Font(Shop->BoldFont(18))
				.HintFont(Shop->BodyFont(9))
				.OnClicked(FSimpleDelegate::CreateSP(this, &SArenaShopDetail::HandleBack))
				.OnHovered(FSimpleDelegate::CreateSP(this, &SArenaShopDetail::HandleHover))
			]
		];

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SArenaGradient).Top(Accent * 0.9f).Bottom(Entry.Color3 * 0.35f)
			]
			+ SOverlay::Slot()
			[
				SNew(SArenaGradient).Top(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)).Bottom(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
			]
			+ SOverlay::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.55f).Padding(FMargin(60.0f, 60.0f, 20.0f, 60.0f))
				[
					SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
					[
						SAssignNew(ArtImage, SImage).Image(ArtBrush.Get()).Visibility(EVisibility::Collapsed)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(0.45f).VAlign(VAlign_Center).Padding(FMargin(20.0f, 60.0f, 80.0f, 60.0f))
				[
					SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed)
					+ SScrollBox::Slot()[Info]
				]
			]
		];

		TWeakPtr<SArenaShopDetail> WeakThis = SharedThis(this);
		Shop->RequestImage(Entry.ImageUrl, WeakThis, [WeakThis](UTexture2D* Texture)
		{
			if (TSharedPtr<SArenaShopDetail> Detail = WeakThis.Pin())
			{
				if (Texture && Detail->ArtImage.IsValid())
				{
					Detail->ArtBrush->SetResourceObject(Texture);
					Detail->ArtBrush->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
					Detail->ArtImage->SetVisibility(EVisibility::HitTestInvisible);
				}
			}
		});
	}

	// Swallow clicks so the tiles under the page never react
	virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
	virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }

private:
	void HandleBuy() { if (UArenaShopWidget* Shop = Owner.Get()) { Shop->NotifyPurchase(Entry); } }
	void HandleBack() { if (UArenaShopWidget* Shop = Owner.Get()) { Shop->CloseDetail(); } }
	void HandleHover() { if (UArenaShopWidget* Shop = Owner.Get()) { Shop->PlayHoverSound(); } }

	FArenaShopEntry Entry;
	TWeakObjectPtr<UArenaShopWidget> Owner;
	TSharedPtr<FSlateBrush> ArtBrush;
	TSharedPtr<FSlateRoundedBoxBrush> PillBrush;
	TSharedPtr<SImage> ArtImage;
};

/** The shop screen: header with the rotation countdown and the wallet, the scrolling sections, ATRÁS and the detail page */
class SArenaShopView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArenaShopView) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UArenaShopWidget>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		using namespace ArenaShopStyle;
		Owner = InArgs._Owner;
		UArenaShopWidget* Shop = Owner.Get();
		SetCanTick(true);
		HeaderBrush = MakeShared<FSlateRoundedBoxBrush>(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), 18.0f);

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SArenaGradient).Top(NavyTop).Bottom(BlueBottom)
			]
			+ SOverlay::Slot()
			[
				SNew(SVerticalBox)
				// Header
				+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(SideMargin, 28.0f, SideMargin, 10.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Title", "Tienda de objetos"))
							.Font(Shop ? Shop->HeadingFont(34) : FCoreStyle::GetDefaultFontStyle("Bold", 34))
							.ColorAndOpacity(White)
							.TransformPolicy(ETextTransformPolicy::ToUpper)
							.RenderTransform(FSlateRenderTransform(FShear2D::FromShearAngles(Italic)))
							.ShadowOffset(FVector2D(0.0f, 2.0f))
							.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(this, &SArenaShopView::GetCountdownText)
							.Font(Shop ? Shop->BoldFont(14) : FCoreStyle::GetDefaultFontStyle("Bold", 14))
							.ColorAndOpacity(Muted)
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBorder).BorderImage(HeaderBrush.Get()).Padding(FMargin(16.0f, 8.0f))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
							[
								SNew(SBox).WidthOverride(22.0f).HeightOverride(22.0f)[SNew(SImage).Image(Shop ? Shop->GetCurrencyBrush() : nullptr)]
							]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(this, &SArenaShopView::GetWalletText)
								.Font(Shop ? Shop->BoldFont(18) : FCoreStyle::GetDefaultFontStyle("Bold", 18))
								.ColorAndOpacity(White)
							]
						]
					]
				]
				// Offers
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SAssignNew(ScrollBox, SScrollBox)
					.Orientation(Orient_Vertical)
					.ScrollBarVisibility(EVisibility::Collapsed)
					.AnimateWheelScrolling(true)
					.WheelScrollMultiplier(2.5f)
				]
			]
			// Status (loading / error)
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SAssignNew(StatusBox, SVerticalBox)
				.Visibility(EVisibility::Collapsed)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SAssignNew(StatusText, STextBlock)
					.Font(Shop ? Shop->BoldFont(20) : FCoreStyle::GetDefaultFontStyle("Bold", 20))
					.ColorAndOpacity(White)
					.Justification(ETextJustify::Center)
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 16.0f, 0.0f, 0.0f)
				[
					SAssignNew(RetryButton, SArenaShopButton)
					.Text(LOCTEXT("Retry", "Reintentar"))
					.Font(Shop ? Shop->BoldFont(16) : FCoreStyle::GetDefaultFontStyle("Bold", 16))
					.HintFont(Shop ? Shop->BodyFont(9) : FCoreStyle::GetDefaultFontStyle("Regular", 9))
					.Visibility(EVisibility::Collapsed)
					.OnClicked(FSimpleDelegate::CreateSP(this, &SArenaShopView::HandleRetry))
					.OnHovered(FSimpleDelegate::CreateSP(this, &SArenaShopView::HandleHover))
				]
			]
			// ATRÁS
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, SideMargin, 30.0f))
			[
				SNew(SArenaShopButton)
				.Text(LOCTEXT("Back", "Atrás"))
				.Hint(LOCTEXT("EscHint", "Esc"))
				.Font(Shop ? Shop->BoldFont(18) : FCoreStyle::GetDefaultFontStyle("Bold", 18))
				.HintFont(Shop ? Shop->BodyFont(9) : FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.OnClicked(FSimpleDelegate::CreateSP(this, &SArenaShopView::HandleBack))
				.OnHovered(FSimpleDelegate::CreateSP(this, &SArenaShopView::HandleHover))
			]
			// Item page
			+ SOverlay::Slot()
			[
				SAssignNew(DetailHost, SBox).Visibility(EVisibility::Collapsed)
			]
			// Toast
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.0f, 96.0f, 0.0f, 0.0f)
			[
				SAssignNew(ToastText, STextBlock)
				.Font(Shop ? Shop->BoldFont(18) : FCoreStyle::GetDefaultFontStyle("Bold", 18))
				.ColorAndOpacity(Yellow)
				.Visibility(EVisibility::HitTestInvisible)
				.RenderOpacity(0.0f)
			]
		];
	}

	void SetShop(const FArenaShopData& Data)
	{
		using namespace ArenaShopStyle;
		UArenaShopWidget* Shop = Owner.Get();
		ScrollBox->ClearChildren();
		if (Shop == nullptr)
		{
			return;
		}
		TWeakPtr<SArenaShopView> WeakThis = SharedThis(this);
		const TAttribute<float> Unit = TAttribute<float>::CreateLambda([WeakThis]()
		{
			const TSharedPtr<SArenaShopView> View = WeakThis.Pin();
			return View.IsValid() ? View->UnitSize : 300.0f;
		});
		const TAttribute<FOptionalSize> Width = TAttribute<FOptionalSize>::CreateLambda([WeakThis]()
		{
			const TSharedPtr<SArenaShopView> View = WeakThis.Pin();
			return FOptionalSize(View.IsValid() ? View->ContentWidth : ContentMaxWidth);
		});

		TSharedRef<SVerticalBox> Column = SNew(SVerticalBox);
		FString LastCategory;
		for (const FArenaShopSection& Section : Data.Sections)
		{
			// A category ("Ídolos", "Calienta motores") groups several sections
			if (!Section.Category.IsEmpty() && Section.Category != LastCategory)
			{
				Column->AddSlot().AutoHeight().Padding(0.0f, 30.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Section.Category))
					.Font(Shop->BoldFont(14))
					.ColorAndOpacity(Muted)
					.TransformPolicy(ETextTransformPolicy::ToUpper)
				];
			}
			LastCategory = Section.Category;

			Column->AddSlot().AutoHeight().Padding(0.0f, Section.Category.IsEmpty() ? 34.0f : 4.0f, 0.0f, 14.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Section.Name.TrimStartAndEnd()))
				.Font(Shop->HeadingFont(30))
				.ColorAndOpacity(White)
				.TransformPolicy(ETextTransformPolicy::ToUpper)
				.RenderTransform(FSlateRenderTransform(FShear2D::FromShearAngles(Italic)))
				.ShadowOffset(FVector2D(0.0f, 2.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f))
			];

			// The wrap box packs the 1x1 / 2x1 / 4x1 tiles into rows of four units, like Fortnite's grid
			TSharedRef<SWrapBox> Grid = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(TileGap, TileGap));
			for (const FArenaShopEntry& Entry : Section.Entries)
			{
				Grid->AddSlot()
				[
					SNew(SArenaShopTile).Entry(Entry).Owner(Owner).UnitSize(Unit)
				];
			}
			Column->AddSlot().AutoHeight()[Grid];
		}
		Column->AddSlot().AutoHeight().Padding(0.0f, 40.0f, 0.0f, 90.0f)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("Footer", "{0} ofertas · datos de fortnite-api.com · la tienda cambia cada día a las 02:00 (hora de España)"), Data.NumEntries))
			.Font(Shop->BodyFont(12))
			.ColorAndOpacity(Dim)
			.Justification(ETextJustify::Center)
		];

		ScrollBox->AddSlot().HAlign(HAlign_Center)
		[
			SNew(SBox).WidthOverride(Width)[Column]
		];
		ScrollBox->ScrollToStart();
	}

	void SetStatus(const FText& Text, bool bRetry)
	{
		if (Text.IsEmpty())
		{
			StatusBox->SetVisibility(EVisibility::Collapsed);
			return;
		}
		StatusText->SetText(Text);
		StatusBox->SetVisibility(EVisibility::SelfHitTestInvisible);
		RetryButton->SetVisibility(bRetry ? EVisibility::Visible : EVisibility::Collapsed);
	}

	void OpenDetail(const FArenaShopEntry& Entry)
	{
		DetailHost->SetContent(SNew(SArenaShopDetail).Entry(Entry).Owner(Owner));
		DetailHost->SetVisibility(EVisibility::Visible);
	}

	void CloseDetail()
	{
		DetailHost->SetContent(SNullWidget::NullWidget);
		DetailHost->SetVisibility(EVisibility::Collapsed);
	}

	bool IsDetailOpen() const { return DetailHost.IsValid() && DetailHost->GetVisibility() != EVisibility::Collapsed; }

	void ShowToast(const FText& Message)
	{
		ToastText->SetText(Message);
		ToastText->SetRenderOpacity(1.0f);
		ToastTimer = 1.8f;
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		using namespace ArenaShopStyle;
		// Tile size follows the width of the screen: four units per row inside the centred column
		ContentWidth = FMath::Clamp(AllottedGeometry.GetLocalSize().X - 2.0f * SideMargin, 640.0f, ContentMaxWidth);
		UnitSize = (ContentWidth - 3.0f * TileGap) / 4.0f;
		if (ToastTimer > 0.0f)
		{
			ToastTimer -= InDeltaTime;
			ToastText->SetRenderOpacity(FMath::Clamp(ToastTimer / 0.4f, 0.0f, 1.0f));
		}
	}

private:
	FText GetCountdownText() const
	{
		const int32 Seconds = FMath::Max(0, FMath::FloorToInt(UArenaShopWidget::SecondsToNextRotation()));
		return FText::Format(LOCTEXT("Countdown", "Nuevos objetos en {0}:{1}:{2}"),
			FText::FromString(FString::Printf(TEXT("%02d"), Seconds / 3600)),
			FText::FromString(FString::Printf(TEXT("%02d"), (Seconds / 60) % 60)),
			FText::FromString(FString::Printf(TEXT("%02d"), Seconds % 60)));
	}

	FText GetWalletText() const
	{
		const UArenaShopWidget* Shop = Owner.Get();
		return FText::FromString(UArenaShopWidget::FormatPrice(Shop ? Shop->WalletVBucks : 0));
	}

	void HandleBack() { if (UArenaShopWidget* Shop = Owner.Get()) { Shop->NotifyBack(); } }
	void HandleRetry() { if (UArenaShopWidget* Shop = Owner.Get()) { Shop->Refresh(true); } }
	void HandleHover() { if (UArenaShopWidget* Shop = Owner.Get()) { Shop->PlayHoverSound(); } }

	TWeakObjectPtr<UArenaShopWidget> Owner;
	TSharedPtr<SScrollBox> ScrollBox;
	TSharedPtr<SVerticalBox> StatusBox;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SArenaShopButton> RetryButton;
	TSharedPtr<SBox> DetailHost;
	TSharedPtr<STextBlock> ToastText;
	TSharedPtr<FSlateRoundedBoxBrush> HeaderBrush;
	float ContentWidth = ArenaShopStyle::ContentMaxWidth;
	float UnitSize = 300.0f;
	float ToastTimer = 0.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// UArenaShopWidget
// ─────────────────────────────────────────────────────────────────────────────

UArenaShopWidget::UArenaShopWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurrencyBrush.DrawAs = ESlateBrushDrawType::Image;
	CurrencyBrush.ImageSize = FVector2D(32.0f, 32.0f);
}

void UArenaShopWidget::SetFonts(UFontFace* Heading, UFontFace* Body, UFontFace* Bold)
{
	HeadingFontFace = Heading;
	BodyFontFace = Body;
	BoldFontFace = Bold;
}

FSlateFontInfo UArenaShopWidget::MakeFont(UFontFace* Face, int32 Size) const
{
	if (Face == nullptr)
	{
		return FCoreStyle::GetDefaultFontStyle("Bold", Size);
	}
	// Same trick as the lobby: the font face is used straight from a composite font, no UFont asset needed
	static TMap<TWeakObjectPtr<UFontFace>, TSharedPtr<FCompositeFont>> Cache;
	TSharedPtr<FCompositeFont>& Font = Cache.FindOrAdd(Face);
	if (!Font.IsValid())
	{
		Font = MakeShared<FStandaloneCompositeFont>();
		FTypefaceEntry Entry(TEXT("Regular"));
		Entry.Font = FFontData(Face);
		Font->DefaultTypeface.Fonts.Add(Entry);
	}
	return FSlateFontInfo(Font, Size);
}

TSharedRef<SWidget> UArenaShopWidget::RebuildWidget()
{
	if (CurrencyIcon)
	{
		CurrencyBrush.SetResourceObject(CurrencyIcon);
	}
	View = SNew(SArenaShopView).Owner(this);
	PushViewData();
	return View.ToSharedRef();
}

void UArenaShopWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	View.Reset();
}

void UArenaShopWidget::NativeDestruct()
{
	ImageWaiters.Empty();
	ImageQueue.Empty();
	Super::NativeDestruct();
}

void UArenaShopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// The lobby builds the shop page while it loads: today's offers are ready by the time TIENDA is pressed.
	// Tile art is only downloaded when a tile is actually drawn.
	Refresh(false);
}

void UArenaShopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Rotation watch: when the UTC day moves past the day of the shop on screen, today's shop is fetched
	DayCheckTimer -= InDeltaTime;
	if (DayCheckTimer <= 0.0f)
	{
		DayCheckTimer = 30.0f;
		if (bLoaded && !bLoading && FDateTime::UtcNow().GetDate() > LoadedDay)
		{
			UE_LOG(LogTemp, Log, TEXT("[ArenaShop] New shop day, refreshing"));
			Refresh(true);
		}
	}
}

double UArenaShopWidget::SecondsToNextRotation()
{
	// Fortnite rotates at 00:00 UTC (02:00 in Spain in summer, 01:00 in winter)
	const FDateTime Now = FDateTime::UtcNow();
	const FDateTime Next = Now.GetDate() + FTimespan::FromDays(1);
	return (Next - Now).GetTotalSeconds();
}

FString UArenaShopWidget::FormatPrice(int32 Price)
{
	// Spanish thousands separator: 2.500
	FString Digits = FString::FromInt(FMath::Abs(Price));
	FString Out;
	for (int32 Index = 0; Index < Digits.Len(); ++Index)
	{
		if (Index > 0 && (Digits.Len() - Index) % 3 == 0)
		{
			Out += TEXT(".");
		}
		Out.AppendChar(Digits[Index]);
	}
	return Price < 0 ? TEXT("-") + Out : Out;
}

FString UArenaShopWidget::CacheDirectory()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ArenaShop"));
}

FString UArenaShopWidget::ShopCacheFile(const FDateTime& Day) const
{
	return FPaths::Combine(CacheDirectory(), FString::Printf(TEXT("shop_%s_%s.json"), *Language, *Day.ToString(TEXT("%Y-%m-%d"))));
}

FString UArenaShopWidget::ImageCacheFile(const FString& Url)
{
	return FPaths::Combine(CacheDirectory(), TEXT("Images"), FMD5::HashAnsiString(*Url) + TEXT(".bin"));
}

void UArenaShopWidget::Refresh(bool bForce)
{
	if (bLoading)
	{
		return;
	}
	const FDateTime Today = FDateTime::UtcNow().GetDate();
	if (!bForce && bLoaded && LoadedDay == Today)
	{
		return;
	}

	// Today's shop already on disk: no network round trip (the lobby opens the shop many times a day)
	FString Cached;
	if (!bForce && FFileHelper::LoadFileToString(Cached, *ShopCacheFile(Today)))
	{
		FArenaShopData Data;
		FString Error;
		if (ParseShop(Cached, Data, Error) && Data.Date.GetDate() == Today)
		{
			Shop = MoveTemp(Data);
			LoadedDay = Today;
			bLoaded = true;
			LastError.Empty();
			PushViewData();
			return;
		}
	}
	StartShopRequest();
}

void UArenaShopWidget::StartShopRequest()
{
	bLoading = true;
	LastError.Empty();
	PushViewData();

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FString::Printf(TEXT("%s?language=%s"), *ShopUrl, *Language));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetTimeout(20.0f);
	Request->OnProcessRequestComplete().BindWeakLambda(this, [this](FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		bLoading = false;
		if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200)
		{
			LastError = Response.IsValid()
				? FString::Printf(TEXT("fortnite-api.com respondió %d"), Response->GetResponseCode())
				: TEXT("sin conexión con fortnite-api.com");
			UE_LOG(LogTemp, Warning, TEXT("[ArenaShop] Shop request failed: %s"), *LastError);
			PushViewData();
			return;
		}

		FArenaShopData Data;
		FString Error;
		if (!ParseShop(Response->GetContentAsString(), Data, Error))
		{
			LastError = Error;
			UE_LOG(LogTemp, Warning, TEXT("[ArenaShop] Could not parse the shop: %s"), *Error);
			PushViewData();
			return;
		}

		const FDateTime Today = FDateTime::UtcNow().GetDate();
		Shop = MoveTemp(Data);
		bLoaded = true;
		LoadedDay = Today;
		if (Shop.Date.GetDate() < Today)
		{
			// Just after the rotation the API can still serve yesterday: look again in five minutes
			LoadedDay = Shop.Date.GetDate();
			DayCheckTimer = 300.0f;
		}

		// Cache today's JSON and drop the older days (and their images, they are no longer needed)
		IFileManager& Files = IFileManager::Get();
		TArray<FString> OldFiles;
		Files.FindFiles(OldFiles, *FPaths::Combine(CacheDirectory(), TEXT("shop_*.json")), true, false);
		const FString TodayFile = FPaths::GetCleanFilename(ShopCacheFile(Shop.Date.GetDate()));
		for (const FString& Old : OldFiles)
		{
			if (Old != TodayFile)
			{
				Files.Delete(*FPaths::Combine(CacheDirectory(), Old));
				Files.DeleteDirectory(*FPaths::Combine(CacheDirectory(), TEXT("Images")), false, true);
			}
		}
		FFileHelper::SaveStringToFile(Response->GetContentAsString(), *ShopCacheFile(Shop.Date.GetDate()));
		UE_LOG(LogTemp, Log, TEXT("[ArenaShop] Shop of %s loaded: %d sections, %d offers"), *Shop.Date.ToString(TEXT("%Y-%m-%d")), Shop.Sections.Num(), Shop.NumEntries);
		PushViewData();
	});
	if (!Request->ProcessRequest())
	{
		bLoading = false;
		LastError = TEXT("no se pudo iniciar la petición");
		PushViewData();
	}
}

void UArenaShopWidget::PushViewData()
{
	if (!View.IsValid())
	{
		return;
	}
	if (bLoaded)
	{
		View->SetShop(Shop);
		View->SetStatus(FText::GetEmpty(), false);
	}
	if (bLoading)
	{
		View->SetStatus(bLoaded ? FText::GetEmpty() : LOCTEXT("Loading", "Cargando la tienda…"), false);
	}
	else if (!LastError.IsEmpty() && !bLoaded)
	{
		View->SetStatus(FText::Format(LOCTEXT("Error", "No se pudo cargar la tienda\n{0}"), FText::FromString(LastError)), true);
	}
	if (!CurrencyIcon && !Shop.VBuckIconUrl.IsEmpty() && CurrencyBrush.GetResourceObject() == nullptr)
	{
		TWeakObjectPtr<UArenaShopWidget> WeakThis(this);
		RequestImage(Shop.VBuckIconUrl, View, [WeakThis](UTexture2D* Texture)
		{
			if (UArenaShopWidget* Self = WeakThis.Get())
			{
				if (Texture)
				{
					Self->CurrencyBrush.SetResourceObject(Texture);
				}
			}
		});
	}
}

namespace
{
	FString JsonString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		FString Value;
		if (Object.IsValid())
		{
			Object->TryGetStringField(Field, Value);
		}
		return Value;
	}

	int32 JsonInt(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32 Default = 0)
	{
		int32 Value = Default;
		if (Object.IsValid())
		{
			Object->TryGetNumberField(Field, Value);
		}
		return Value;
	}

	TSharedPtr<FJsonObject> JsonObject(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Object.IsValid() && Object->TryGetObjectField(Field, Child) && Child)
		{
			return *Child;
		}
		return nullptr;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Object.IsValid() && Object->TryGetArrayField(Field, Array))
		{
			return Array;
		}
		return nullptr;
	}

	/** "Size_2_x_1" → 2 columns, 1 row */
	void ParseTileSize(const FString& Size, int32& Columns, int32& Rows)
	{
		Columns = 1;
		Rows = 1;
		TArray<FString> Parts;
		Size.ParseIntoArray(Parts, TEXT("_"));
		if (Parts.Num() >= 4 && Parts[0] == TEXT("Size"))
		{
			Columns = FMath::Clamp(FCString::Atoi(*Parts[1]), 1, 4);
			Rows = FMath::Clamp(FCString::Atoi(*Parts[3]), 1, 2);
		}
	}

	/** Cosmetics, tracks, cars and instruments share most fields; the rest is read where it exists */
	void ReadItems(const TSharedPtr<FJsonObject>& Entry, const TCHAR* Field, const TCHAR* KindFallback, TArray<FArenaShopItem>& Items)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = JsonArray(Entry, Field);
		if (Array == nullptr)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Object.IsValid())
			{
				continue;
			}
			FArenaShopItem Item;
			Item.Name = JsonString(Object, TEXT("name"));
			if (Item.Name.IsEmpty())
			{
				Item.Name = JsonString(Object, TEXT("title"));
			}
			Item.Description = JsonString(Object, TEXT("description"));
			if (Item.Description.IsEmpty())
			{
				Item.Description = JsonString(Object, TEXT("artist"));
			}
			Item.TypeName = JsonString(JsonObject(Object, TEXT("type")), TEXT("displayValue"));
			if (Item.TypeName.IsEmpty())
			{
				Item.TypeName = KindFallback;
			}
			Item.RarityName = JsonString(JsonObject(Object, TEXT("rarity")), TEXT("displayValue"));
			Item.RarityValue = JsonString(JsonObject(Object, TEXT("rarity")), TEXT("value"));
			Item.SeriesName = JsonString(JsonObject(Object, TEXT("series")), TEXT("value"));
			Item.SetText = JsonString(JsonObject(Object, TEXT("set")), TEXT("text"));
			Item.IntroductionText = JsonString(JsonObject(Object, TEXT("introduction")), TEXT("text"));
			const TSharedPtr<FJsonObject> Images = JsonObject(Object, TEXT("images"));
			Item.IconUrl = JsonString(Images, TEXT("featured"));
			if (Item.IconUrl.IsEmpty()) { Item.IconUrl = JsonString(Images, TEXT("large")); }
			if (Item.IconUrl.IsEmpty()) { Item.IconUrl = JsonString(Images, TEXT("icon")); }
			if (Item.IconUrl.IsEmpty()) { Item.IconUrl = JsonString(Images, TEXT("small")); }
			if (Item.IconUrl.IsEmpty()) { Item.IconUrl = JsonString(Object, TEXT("albumArt")); }
			Items.Add(MoveTemp(Item));
		}
	}
}

bool UArenaShopWidget::ParseShop(const FString& Json, FArenaShopData& Out, FString& Error) const
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Error = TEXT("respuesta no válida");
		return false;
	}
	if (JsonInt(Root, TEXT("status"), 200) != 200)
	{
		Error = JsonString(Root, TEXT("error"));
		return false;
	}
	const TSharedPtr<FJsonObject> Data = JsonObject(Root, TEXT("data"));
	if (!Data.IsValid())
	{
		Error = TEXT("la respuesta no trae datos");
		return false;
	}

	Out.Hash = JsonString(Data, TEXT("hash"));
	Out.VBuckIconUrl = JsonString(Data, TEXT("vbuckIcon"));
	if (!FDateTime::ParseIso8601(*JsonString(Data, TEXT("date")), Out.Date))
	{
		Out.Date = FDateTime::UtcNow().GetDate();
	}

	TMap<FString, FArenaShopSection> Sections;
	const TArray<TSharedPtr<FJsonValue>>* Entries = JsonArray(Data, TEXT("entries"));
	if (Entries == nullptr)
	{
		Error = TEXT("la tienda no tiene ofertas");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Entries)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Object.IsValid())
		{
			continue;
		}
		FArenaShopEntry Entry;
		Entry.OfferId = JsonString(Object, TEXT("offerId"));
		Entry.RegularPrice = JsonInt(Object, TEXT("regularPrice"));
		Entry.FinalPrice = JsonInt(Object, TEXT("finalPrice"), Entry.RegularPrice);
		Entry.SortPriority = JsonInt(Object, TEXT("sortPriority"));
		Entry.BannerText = JsonString(JsonObject(Object, TEXT("banner")), TEXT("value")).Replace(TEXT("\u00A0"), TEXT(" "));
		ParseTileSize(JsonString(Object, TEXT("tileSize")), Entry.Columns, Entry.Rows);

		const TSharedPtr<FJsonObject> Colors = JsonObject(Object, TEXT("colors"));
		ReadItems(Object, TEXT("brItems"), TEXT(""), Entry.Items);
		ReadItems(Object, TEXT("tracks"), *LOCTEXT("KindTrack", "Pista de improvisación").ToString(), Entry.Items);
		ReadItems(Object, TEXT("cars"), *LOCTEXT("KindCar", "Vehículo").ToString(), Entry.Items);
		ReadItems(Object, TEXT("instruments"), *LOCTEXT("KindInstrument", "Instrumento").ToString(), Entry.Items);
		ReadItems(Object, TEXT("legoKits"), TEXT("LEGO"), Entry.Items);
		const FLinearColor Fallback = Entry.Items.IsValidIndex(0) ? ArenaShopStyle::RarityColor(Entry.Items[0].RarityValue) : Entry.Color1;
		Entry.Color1 = ArenaShopStyle::FromHex(JsonString(Colors, TEXT("color1")), Fallback);
		Entry.Color2 = ArenaShopStyle::FromHex(JsonString(Colors, TEXT("color2")), Entry.Color1 * 0.6f);
		Entry.Color3 = ArenaShopStyle::FromHex(JsonString(Colors, TEXT("color3")), Entry.Color1 * 0.25f);
		Entry.TextBackground = ArenaShopStyle::FromHex(JsonString(Colors, TEXT("textBackgroundColor")), FLinearColor::Black);

		const TSharedPtr<FJsonObject> Bundle = JsonObject(Object, TEXT("bundle"));
		if (Bundle.IsValid())
		{
			Entry.bBundle = true;
			Entry.Title = JsonString(Bundle, TEXT("name"));
			Entry.BundleInfo = JsonString(Bundle, TEXT("info"));
		}
		if (Entry.Title.IsEmpty() && Entry.Items.IsValidIndex(0))
		{
			Entry.Title = Entry.Items[0].Name;
		}
		if (Entry.Title.IsEmpty())
		{
			// "[VIRTUAL]1 x Bear Brained for 300 MtxCurrency" → "Bear Brained"
			FString DevName = JsonString(Object, TEXT("devName"));
			int32 Start = DevName.Find(TEXT(" x "));
			int32 End = DevName.Find(TEXT(" for "), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (Start != INDEX_NONE && End != INDEX_NONE && End > Start)
			{
				DevName = DevName.Mid(Start + 3, End - Start - 3);
			}
			Entry.Title = DevName;
		}

		// Tile art: the render image of the new display asset is the finished tile (background included)
		const TArray<TSharedPtr<FJsonValue>>* Renders = JsonArray(JsonObject(Object, TEXT("newDisplayAsset")), TEXT("renderImages"));
		if (Renders && Renders->Num() > 0 && (*Renders)[0].IsValid())
		{
			Entry.ImageUrl = JsonString((*Renders)[0]->AsObject(), TEXT("image"));
			Entry.bImageIsTileArt = true;
		}
		if (Entry.ImageUrl.IsEmpty() && Bundle.IsValid())
		{
			Entry.ImageUrl = JsonString(Bundle, TEXT("image"));
			Entry.bImageIsTileArt = !Entry.ImageUrl.IsEmpty();
		}
		if (Entry.ImageUrl.IsEmpty() && Entry.Items.IsValidIndex(0))
		{
			Entry.ImageUrl = Entry.Items[0].IconUrl;
			// Album covers are full square pictures: let them fill the tile
			Entry.bImageIsTileArt = Entry.ImageUrl.Contains(TEXT("/tracks/"));
		}

		const TSharedPtr<FJsonObject> Layout = JsonObject(Object, TEXT("layout"));
		FString SectionId = JsonString(Layout, TEXT("id"));
		if (SectionId.IsEmpty())
		{
			SectionId = TEXT("__other");
		}
		FArenaShopSection& Section = Sections.FindOrAdd(SectionId);
		if (Section.Id.IsEmpty())
		{
			Section.Id = SectionId;
			Section.Name = Layout.IsValid() ? JsonString(Layout, TEXT("name")) : LOCTEXT("OtherOffers", "Más ofertas").ToString();
			Section.Category = JsonString(Layout, TEXT("category"));
			Section.Index = Layout.IsValid() ? JsonInt(Layout, TEXT("index"), 9999) : 9999;
		}
		Section.Entries.Add(MoveTemp(Entry));
		++Out.NumEntries;
	}

	Sections.GenerateValueArray(Out.Sections);
	Out.Sections.Sort([](const FArenaShopSection& A, const FArenaShopSection& B) { return A.Index < B.Index; });
	for (FArenaShopSection& Section : Out.Sections)
	{
		// Fortnite lists the highest priority first
		Section.Entries.StableSort([](const FArenaShopEntry& A, const FArenaShopEntry& B) { return A.SortPriority > B.SortPriority; });
	}
	return true;
}

// ── Images ──────────────────────────────────────────────────────────────

UTexture2D* UArenaShopWidget::FindCachedImage(const FString& Url) const
{
	const TObjectPtr<UTexture2D>* Texture = Textures.Find(Url);
	return Texture ? Texture->Get() : nullptr;
}

void UArenaShopWidget::RequestImage(const FString& Url, TWeakPtr<SWidget> Owner, TFunction<void(UTexture2D*)> Callback)
{
	if (Url.IsEmpty())
	{
		return;
	}
	if (UTexture2D* Cached = FindCachedImage(Url))
	{
		Callback(Cached);
		return;
	}
	const bool bAlreadyWanted = ImageWaiters.Contains(Url);
	ImageWaiters.FindOrAdd(Url).Add({ Owner, MoveTemp(Callback) });
	if (!bAlreadyWanted)
	{
		ImageQueue.Add(Url);
		PumpImageQueue();
	}
}

void UArenaShopWidget::PumpImageQueue()
{
	// A handful at a time keeps the shop responsive while it scrolls through hundreds of tiles
	constexpr int32 MaxParallel = 4;
	while (ActiveImageRequests < MaxParallel && ImageQueue.Num() > 0)
	{
		// Most recent first: the tiles on screen now matter more than the ones scrolled past
		const FString Url = ImageQueue.Pop(EAllowShrinking::No);
		if (!ImageWaiters.Contains(Url))
		{
			continue;
		}
		StartImageRequest(Url);
	}
}

void UArenaShopWidget::StartImageRequest(const FString& Url)
{
	++ActiveImageRequests;
	TArray<uint8> Cached;
	if (FFileHelper::LoadFileToArray(Cached, *ImageCacheFile(Url)) && Cached.Num() > 0)
	{
		DecodeImage(Url, MoveTemp(Cached));
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetTimeout(20.0f);
	Request->OnProcessRequestComplete().BindWeakLambda(this, [this, Url](FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200 || Response->GetContent().IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[ArenaShop] Could not download %s"), *Url);
			FinishImage(Url, nullptr);
			return;
		}
		FFileHelper::SaveArrayToFile(Response->GetContent(), *ImageCacheFile(Url));
		DecodeImage(Url, Response->GetContent());
	});
	if (!Request->ProcessRequest())
	{
		FinishImage(Url, nullptr);
	}
}

void UArenaShopWidget::DecodeImage(const FString& Url, TArray<uint8> Bytes)
{
	// PNG decoding and the downscale run off the game thread; only the texture creation happens on it
	TWeakObjectPtr<UArenaShopWidget> WeakThis(this);
	const int32 MaxSize = MaxTextureSize;
	Async(EAsyncExecution::ThreadPool, [WeakThis, Url, Bytes = MoveTemp(Bytes), MaxSize]()
	{
		TSharedPtr<FImage, ESPMode::ThreadSafe> Image = MakeShared<FImage, ESPMode::ThreadSafe>();
		bool bOk = FImageUtils::DecompressImage(Bytes.GetData(), Bytes.Num(), *Image);
		if (bOk && (Image->SizeX > MaxSize || Image->SizeY > MaxSize))
		{
			const float Scale = static_cast<float>(MaxSize) / FMath::Max(Image->SizeX, Image->SizeY);
			TSharedPtr<FImage, ESPMode::ThreadSafe> Small = MakeShared<FImage, ESPMode::ThreadSafe>();
			Image->ResizeTo(*Small, FMath::Max(1, FMath::RoundToInt(Image->SizeX * Scale)), FMath::Max(1, FMath::RoundToInt(Image->SizeY * Scale)), ERawImageFormat::BGRA8, EGammaSpace::sRGB);
			Image = Small;
		}
		AsyncTask(ENamedThreads::GameThread, [WeakThis, Url, Image, bOk]()
		{
			UArenaShopWidget* Self = WeakThis.Get();
			if (Self == nullptr)
			{
				return;
			}
			UTexture2D* Texture = bOk ? FImageUtils::CreateTexture2DFromImage(*Image) : nullptr;
			if (Texture == nullptr)
			{
				UE_LOG(LogTemp, Warning, TEXT("[ArenaShop] Could not decode %s"), *Url);
			}
			Self->FinishImage(Url, Texture);
		});
	});
}

void UArenaShopWidget::FinishImage(const FString& Url, UTexture2D* Texture)
{
	ActiveImageRequests = FMath::Max(0, ActiveImageRequests - 1);
	if (Texture)
	{
		Textures.Add(Url, Texture);
	}
	TArray<FImageWaiter> Waiters;
	ImageWaiters.RemoveAndCopyValue(Url, Waiters);
	for (FImageWaiter& Waiter : Waiters)
	{
		if (Texture && Waiter.Owner.IsValid() && Waiter.Callback)
		{
			Waiter.Callback(Texture);
		}
	}
	PumpImageQueue();
}

// ── Navigation ───────────────────────────────────────────────────────────

void UArenaShopWidget::ShowDetail(const FArenaShopEntry& Entry)
{
	if (View.IsValid())
	{
		View->OpenDetail(Entry);
	}
}

void UArenaShopWidget::CloseDetail()
{
	if (View.IsValid())
	{
		View->CloseDetail();
	}
}

bool UArenaShopWidget::IsDetailOpen() const
{
	return View.IsValid() && View->IsDetailOpen();
}

bool UArenaShopWidget::HandleBack()
{
	if (IsDetailOpen())
	{
		CloseDetail();
		return true;
	}
	return false;
}

void UArenaShopWidget::NotifyBack()
{
	if (!HandleBack())
	{
		OnBackPressed.Broadcast();
	}
}

void UArenaShopWidget::NotifyPurchase(const FArenaShopEntry& Entry)
{
	if (View.IsValid())
	{
		View->ShowToast(FText::Format(LOCTEXT("PurchaseToast", "Compra de {0} no disponible en esta recreación"), FText::FromString(Entry.Title)));
	}
}

void UArenaShopWidget::PlayHoverSound()
{
	ArenaUISounds::PlayHover(this);
}

#undef LOCTEXT_NAMESPACE

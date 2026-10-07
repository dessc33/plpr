// Fortnite item shop recreated in Slate. The offers come from https://fortnite-api.com/v2/shop at run time, so the
// shop shows whatever Fortnite sells today (the rotation happens at 00:00 UTC, 02:00 in Spain) and never ships a
// fixed catalogue in the code.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "ArenaShopWidget.generated.h"

class UFontFace;
class UTexture2D;
class SArenaShopView;

/** One cosmetic inside an offer (outfit, pickaxe, track, car...) */
struct FArenaShopItem
{
	FString Name;
	FString Description;
	FString TypeName;
	FString RarityName;
	/** rarity value of the API (common, rare, epic, legendary, icon, marvel, dark, gaminglegends...) */
	FString RarityValue;
	FString SeriesName;
	FString SetText;
	FString IntroductionText;
	FString IconUrl;
};

/** One tile of the shop */
struct FArenaShopEntry
{
	FString OfferId;
	FString Title;
	/** Full tile art (render image of the new display asset) or the icon of the first item when there is none */
	FString ImageUrl;
	/** True when ImageUrl is a complete tile render (fills the tile), false when it is an icon drawn over the colours */
	bool bImageIsTileArt = false;
	int32 RegularPrice = 0;
	int32 FinalPrice = 0;
	/** Discount / "new" banner text shown as a white pill over the name */
	FString BannerText;
	FLinearColor Color1 = FLinearColor(0.08f, 0.08f, 0.10f);
	FLinearColor Color2 = FLinearColor(0.05f, 0.05f, 0.07f);
	FLinearColor Color3 = FLinearColor(0.02f, 0.02f, 0.03f);
	FLinearColor TextBackground = FLinearColor(0.0f, 0.0f, 0.0f);
	/** Width and height in grid units (Size_2_x_1 = 2 wide, 1 tall) */
	int32 Columns = 1;
	int32 Rows = 1;
	bool bBundle = false;
	FString BundleInfo;
	int32 SortPriority = 0;
	TArray<FArenaShopItem> Items;
};

/** A row group of the shop (layout of the API: "Eminem", "Lotes y ofertas especiales"...) */
struct FArenaShopSection
{
	FString Id;
	FString Name;
	FString Category;
	int32 Index = 0;
	TArray<FArenaShopEntry> Entries;
};

struct FArenaShopData
{
	FString Hash;
	/** Day the shop belongs to (UTC midnight) */
	FDateTime Date;
	FString VBuckIconUrl;
	TArray<FArenaShopSection> Sections;
	int32 NumEntries = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArenaShopBackEvent);

/**
 * Hosts the Slate shop view (SArenaShopView) inside a UUserWidget so the lobby can place it like any other page.
 * Owns the download of the shop JSON, the image cache (memory + Saved/ArenaShop on disk) and the daily refresh.
 */
UCLASS(Blueprintable)
class ARENA_API UArenaShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UArenaShopWidget(const FObjectInitializer& ObjectInitializer);

	/** Fired when ATRÁS is pressed (or ESC with nothing else to close) */
	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FArenaShopBackEvent OnBackPressed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FString ShopUrl = TEXT("https://fortnite-api.com/v2/shop");

	/** Language of names and descriptions (fortnite-api language codes: es, en, fr...) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FString Language = TEXT("es");

	/** Same font faces as the lobby (Burbank-like), set by the lobby widget */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Style")
	TObjectPtr<UFontFace> HeadingFontFace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Style")
	TObjectPtr<UFontFace> BodyFontFace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Style")
	TObjectPtr<UFontFace> BoldFontFace;

	/** V-Bucks icon; when unset the icon of the API is downloaded */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Style")
	TObjectPtr<UTexture2D> CurrencyIcon;

	/** V-Bucks shown in the wallet of the header */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 WalletVBucks = 0;

	/** Longest side of the textures kept in memory (tile art is 1024 px, far more than a tile needs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ClampMin = "128", ClampMax = "2048"))
	int32 MaxTextureSize = 640;

	void SetFonts(UFontFace* Heading, UFontFace* Body, UFontFace* Bold);

	/** Downloads today's shop (or reads it from the disk cache). bForce ignores the cache */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void Refresh(bool bForce = false);

	/** ESC / ATRÁS: closes the item detail when it is open and returns true, otherwise returns false */
	bool HandleBack();

	bool IsDetailOpen() const;

	// ── Services used by the Slate widgets ─────────────────────────
	FSlateFontInfo MakeFont(UFontFace* Face, int32 Size) const;
	FSlateFontInfo HeadingFont(int32 Size) const { return MakeFont(HeadingFontFace ? HeadingFontFace : BoldFontFace, Size); }
	FSlateFontInfo BodyFont(int32 Size) const { return MakeFont(BodyFontFace, Size); }
	FSlateFontInfo BoldFont(int32 Size) const { return MakeFont(BoldFontFace ? BoldFontFace : BodyFontFace, Size); }

	/** Asks for a texture; Callback runs on the game thread once it is ready (immediately when cached). Owner keeps dead widgets from being called */
	void RequestImage(const FString& Url, TWeakPtr<SWidget> Owner, TFunction<void(UTexture2D*)> Callback);
	UTexture2D* FindCachedImage(const FString& Url) const;
	const FSlateBrush* GetCurrencyBrush() const { return &CurrencyBrush; }
	void PlayHoverSound();
	void ShowDetail(const FArenaShopEntry& Entry);
	void CloseDetail();
	void NotifyBack();
	void NotifyPurchase(const FArenaShopEntry& Entry);
	/** Seconds until the next rotation (00:00 UTC) */
	static double SecondsToNextRotation();
	static FString FormatPrice(int32 Price);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

private:
	TSharedPtr<SArenaShopView> View;

	FArenaShopData Shop;
	bool bLoading = false;
	bool bLoaded = false;
	FString LastError;
	/** UTC day of the shop on screen; the view refreshes when the real day moves past it */
	FDateTime LoadedDay;
	float DayCheckTimer = 0.0f;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> Textures;

	struct FImageWaiter
	{
		TWeakPtr<SWidget> Owner;
		TFunction<void(UTexture2D*)> Callback;
	};
	TMap<FString, TArray<FImageWaiter>> ImageWaiters;
	TArray<FString> ImageQueue;
	int32 ActiveImageRequests = 0;
	FSlateBrush CurrencyBrush;

	void PumpImageQueue();
	void StartImageRequest(const FString& Url);
	void DecodeImage(const FString& Url, TArray<uint8> Bytes);
	void FinishImage(const FString& Url, UTexture2D* Texture);
	void PushViewData();
	bool ParseShop(const FString& Json, FArenaShopData& Out, FString& Error) const;
	static FString CacheDirectory();
	FString ShopCacheFile(const FDateTime& Day) const;
	static FString ImageCacheFile(const FString& Url);
	void StartShopRequest();
};

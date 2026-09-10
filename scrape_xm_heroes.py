#!/usr/bin/env python3
"""
Sony XM Device Hero Image Scraper and Downloader
=================================================
Scrapes and downloads official hero product images for Sony XM (1000X series)
headphone and earbud devices directly from the Sony website and Sony's official
digital asset management CDN.

Features:
- Supports all XM series models (WH-1000XM2..M6, WF-1000X..XM6, WI-1000X..XM2, MDR-1000X)
  and allows custom Sony model specifications.
- Scrapes product page HTML (og:image, PDP header images, and outline renders).
- Supports Sony Scene7 CDN mode for official transparent RGBA PNG renders (720px),
  matching the exact format used in SonyBridge desktop client.
- Auto-naming follows project slug convention: `wh-1000xm5.png`, `wf-1000xm4.jpg`, etc.
- Optional transparent border autocrop (--trim) using Pillow.
- Dry-run mode to inspect discovered URLs.
- Direct update option (--update-resources) to populate Client/resources/devices.
"""

import argparse
import os
import re
import sys
from pathlib import Path
from urllib.parse import urljoin, urlparse

try:
    import requests
except ImportError:
    print("Error: 'requests' library is required. Install with: pip install requests", file=sys.stderr)
    sys.exit(1)

try:
    from bs4 import BeautifulSoup
    HAS_BS4 = True
except ImportError:
    HAS_BS4 = False

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False


# Default list of Sony XM (1000X series) device models
DEFAULT_XM_MODELS = [
    # Over-Ear (Headband)
    "WH-1000XM6",
    "WH-1000XM5",
    "WH-1000XM4",
    "WH-1000XM3",
    "WH-1000XM2",
    "MDR-1000X",
    # Truly Wireless (Earbuds)
    "WF-1000XM6",
    "WF-1000XM5",
    "WF-1000XM4",
    "WF-1000XM3",
    "WF-1000X",
    # Behind-the-Neck (Neckband)
    "WI-1000XM2",
    "WI-1000X",
]

DEFAULT_HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/124.0.0.0 Safari/537.36"
    ),
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
    "Accept-Language": "en-US,en;q=0.9,ja;q=0.8",
}


def normalize_slug(model: str) -> str:
    """
    Normalizes a model name into standard lowercase hyphenated slug
    (e.g., 'WH-1000XM5' -> 'wh-1000xm5').
    """
    slug = model.strip().lower()
    slug = re.sub(r"[\s_]+", "-", slug)
    return slug


def format_model_name(model: str) -> str:
    """
    Attempts to format a raw string into standard Sony XM casing
    (e.g., 'wh1000xm5' -> 'WH-1000XM5').
    """
    m = model.strip()
    # Match standard patterns like wh1000xm5 or wh-1000xm5
    match = re.match(r"^([a-zA-Z]{2,3})[-_]?([0-9]{4}[a-zA-Z0-9]+)$", m)
    if match:
        prefix, num = match.group(1).upper(), match.group(2).upper()
        return f"{prefix}-{num}"
    return m.upper()


def scrape_web_hero(model: str, session: requests.Session) -> dict:
    """
    Scrapes the official Sony product page for a model and extracts the hero image URL.
    """
    model_formatted = format_model_name(model)
    page_url = f"https://www.sony.jp/headphone/products/{model_formatted}/"

    try:
        resp = session.get(page_url, headers=DEFAULT_HEADERS, timeout=12)
        if resp.status_code == 404:
            # Try lowercase URL if uppercase 404s
            page_url = f"https://www.sony.jp/headphone/products/{model.strip().lower()}/"
            resp = session.get(page_url, headers=DEFAULT_HEADERS, timeout=12)

        if resp.status_code != 200:
            return {"error": f"HTTP {resp.status_code} on product page", "page_url": page_url}

        html = resp.text
        hero_url = None

        if HAS_BS4:
            soup = BeautifulSoup(html, "html.parser")
            # 1. OpenGraph meta tag (usually points directly to full-size product image)
            og = soup.find("meta", property="og:image")
            if og and og.get("content"):
                hero_url = og["content"].strip()

            # 2. Main PDP header image in CategoryNav
            if not hero_url:
                pdp = soup.find("img", class_="CategoryNav__PdpHeaderImage")
                if pdp and pdp.get("src"):
                    hero_url = urljoin(page_url, pdp["src"])

            # 3. Product outline showcase image
            if not hero_url:
                outline = soup.find("div", class_="s5-outline__image")
                if outline and outline.find("img"):
                    hero_url = urljoin(page_url, outline.find("img")["src"])

            # 4. Fallback search through all img tags for picture/
            if not hero_url:
                for img in soup.find_all("img"):
                    src = img.get("src") or ""
                    if "products/picture" in src and not any(x in src for x in ["logo", "icon", "bnr"]):
                        hero_url = urljoin(page_url, src)
                        break
        else:
            # Lightweight regex fallback when bs4 is not available
            og_match = re.search(r'<meta[^>]+property=["\']og:image["\'][^>]+content=["\']([^"\']+)["\']', html, re.I)
            if not og_match:
                og_match = re.search(r'<meta[^>]+content=["\']([^"\']+)["\'][^>]+property=["\']og:image["\']', html, re.I)
            if og_match:
                hero_url = og_match.group(1).strip()
            else:
                img_match = re.search(r'src=["\'](/products/picture/[^"\']+\.(?:jpg|jpeg|png))["\']', html, re.I)
                if img_match:
                    hero_url = urljoin(page_url, img_match.group(1))

        if not hero_url:
            return {"error": "Could not find hero image on page", "page_url": page_url}

        # Check for higher-resolution variant if a thumbnail was found
        high_res_url = hero_url
        if "/small/" in hero_url or "/middle/" in hero_url:
            candidate = re.sub(r"/picture/(?:small|middle)/", "/picture/", hero_url)
            try:
                head_resp = session.head(candidate, headers=DEFAULT_HEADERS, timeout=5)
                if head_resp.status_code == 200:
                    high_res_url = candidate
            except Exception:
                pass

        return {
            "model": model_formatted,
            "hero_url": high_res_url,
            "page_url": page_url,
            "type": "web",
        }

    except Exception as e:
        return {"error": str(e), "page_url": page_url}


def find_scene7_render(model: str, session: requests.Session, width: int = 720) -> dict:
    """
    Finds the high-resolution transparent cutout render from Sony's Adobe Scene7 DAM.
    """
    formatted = format_model_name(model)
    slug = normalize_slug(model)
    candidates = [
        formatted,
        slug,
        model.strip(),
        formatted.replace("-", "_"),
        formatted.replace("-", ""),
    ]

    base_cdn = "https://sony.scene7.com/is/image/sonyglobalsolutions/"

    for candidate in candidates:
        render_url = f"{base_cdn}{candidate}?fmt=png-alpha&wid={width}"
        try:
            r = session.head(render_url, headers=DEFAULT_HEADERS, timeout=6)
            if r.status_code == 200 and "image" in r.headers.get("content-type", ""):
                return {
                    "model": formatted,
                    "hero_url": render_url,
                    "type": "render",
                }
        except Exception:
            continue

    return {"error": f"No Scene7 transparent render found for {model}"}


def download_file(
    url: str,
    dest_path: Path,
    session: requests.Session,
    trim: bool = False,
    force: bool = False,
) -> tuple[bool, str]:
    """
    Downloads image from URL to dest_path. Optionally trims transparent borders using PIL.
    """
    if dest_path.exists() and not force:
        return True, f"Skipped (already exists: {dest_path.name})"

    dest_path.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = dest_path.with_suffix(dest_path.suffix + ".tmp")

    try:
        with session.get(url, headers=DEFAULT_HEADERS, stream=True, timeout=20) as r:
            r.raise_for_status()
            content_type = r.headers.get("content-type", "")
            if "image" not in content_type and "octet-stream" not in content_type:
                return False, f"Unexpected Content-Type: {content_type}"

            with open(tmp_path, "wb") as f:
                for chunk in r.iter_content(chunk_size=16384):
                    if chunk:
                        f.write(chunk)

        # Optional post-processing: trim transparent borders
        if trim and HAS_PIL and dest_path.suffix.lower() == ".png":
            try:
                img = Image.open(tmp_path)
                if img.mode in ("RGBA", "LA") or (img.mode == "P" and "transparency" in img.info):
                    bbox = img.convert("RGBA").getbbox()
                    if bbox:
                        img = img.crop(bbox)
                        img.save(tmp_path, format="PNG", optimize=True)
            except Exception as pe:
                pass  # Fall back to untrimmed file

        tmp_path.replace(dest_path)
        size_kb = dest_path.stat().st_size / 1024

        dim_str = ""
        if HAS_PIL:
            try:
                with Image.open(dest_path) as im:
                    dim_str = f" [{im.width}x{im.height}px, {im.mode}]"
            except Exception:
                pass

        return True, f"Downloaded {dest_path.name} ({size_kb:.1f} KB{dim_str})"

    except Exception as e:
        if tmp_path.exists():
            tmp_path.unlink()
        return False, f"Download failed: {e}"


def main():
    parser = argparse.ArgumentParser(
        description="Scrape and download hero images for Sony XM devices.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Download official transparent renders for all XM devices into downloads/
  python3 scrape_xm_heroes.py --transparent

  # Scrape product page hero images from Sony website
  python3 scrape_xm_heroes.py --source web

  # Download both web hero images and transparent renders for specific models
  python3 scrape_xm_heroes.py --models WH-1000XM5 WF-1000XM5 --source both

  # Directly update the SonyBridge app's resources/devices folder
  python3 scrape_xm_heroes.py --transparent --trim --update-resources

  # Dry-run to preview image URLs without downloading
  python3 scrape_xm_heroes.py --dry-run
        """,
    )

    parser.add_argument(
        "--models",
        "-m",
        nargs="+",
        default=DEFAULT_XM_MODELS,
        help="Model names or slugs to scrape (default: all Sony XM / 1000X models)",
    )
    parser.add_argument(
        "--source",
        "-s",
        choices=["web", "render", "both"],
        default="both",
        help="Image source: 'web' (scraped from Sony product page HTML), "
        "'render' (Sony Scene7 transparent PNG cutout), or 'both' (default: both)",
    )
    parser.add_argument(
        "--output-dir",
        "-o",
        type=str,
        default="downloads/xm_heroes",
        help="Directory to save downloaded images (default: downloads/xm_heroes)",
    )
    parser.add_argument(
        "--transparent",
        "-t",
        action="store_true",
        help="Shortcut for --source render: downloads transparent cutout PNGs",
    )
    parser.add_argument(
        "--wid",
        "--width",
        type=int,
        default=720,
        help="Target image width for Scene7 renders in pixels (default: 720)",
    )
    parser.add_argument(
        "--trim",
        action="store_true",
        help="Trim transparent margins using Pillow (for PNG renders)",
    )
    parser.add_argument(
        "--force",
        "-f",
        action="store_true",
        help="Overwrite already downloaded files",
    )
    parser.add_argument(
        "--dry-run",
        "-d",
        action="store_true",
        help="Scrape and inspect URLs without downloading files",
    )
    parser.add_argument(
        "--update-resources",
        action="store_true",
        help="Save directly into 'Client/resources/devices/' for the desktop app",
    )
    parser.add_argument(
        "--quiet",
        "-q",
        action="store_true",
        help="Reduce output verbosity",
    )

    args = parser.parse_args()

    # Apply shortcuts
    source = "render" if args.transparent else args.source

    if args.update_resources:
        repo_root = Path(__file__).resolve().parent
        out_dir = repo_root / "Client" / "resources" / "devices"
    else:
        out_dir = Path(args.output_dir)

    print("=" * 68)
    print("  Sony XM Hero Image Scraper & Downloader")
    print("=" * 68)
    print(f"Target Models : {len(args.models)} devices")
    print(f"Source Mode   : {source}")
    print(f"Destination   : {out_dir}")
    if args.dry_run:
        print("Dry Run       : True (no files will be written)")
    print("-" * 68)

    session = requests.Session()
    success_count = 0
    fail_count = 0

    for model in args.models:
        slug = normalize_slug(model)
        formatted = format_model_name(model)
        print(f"\n[+] Processing: {formatted} ({slug})")

        targets_to_download = []

        # 1. Scrape from Sony website product page
        if source in ("web", "both"):
            web_info = scrape_web_hero(model, session)
            if "error" in web_info:
                print(f"    - Web Scraper   : FAILED ({web_info['error']})")
            else:
                hero_url = web_info["hero_url"]
                parsed_ext = Path(urlparse(hero_url).path).suffix or ".jpg"
                filename = f"{slug}_web{parsed_ext}" if source == "both" else f"{slug}{parsed_ext}"
                dest = out_dir / filename
                print(f"    - Web Scraper   : Found -> {hero_url}")
                targets_to_download.append((hero_url, dest, False))

        # 2. Query official transparent render from Sony Scene7 DAM
        if source in ("render", "both"):
            render_info = find_scene7_render(model, session, width=args.wid)
            if "error" in render_info:
                print(f"    - Scene7 Render : FAILED ({render_info['error']})")
            else:
                render_url = render_info["hero_url"]
                filename = f"{slug}.png"
                dest = out_dir / filename
                print(f"    - Scene7 Render : Found -> {render_url}")
                targets_to_download.append((render_url, dest, args.trim))

        # Execute downloads or dry-run
        if not targets_to_download:
            fail_count += 1
            continue

        for url, dest_path, do_trim in targets_to_download:
            if args.dry_run:
                print(f"    -> [DRY-RUN] Would download to: {dest_path}")
                success_count += 1
            else:
                ok, msg = download_file(
                    url=url,
                    dest_path=dest_path,
                    session=session,
                    trim=do_trim,
                    force=args.force,
                )
                if ok:
                    print(f"    -> {msg}")
                    success_count += 1
                else:
                    print(f"    -> ERROR: {msg}")
                    fail_count += 1

    print("\n" + "=" * 68)
    print(f"Completed: {success_count} succeeded, {fail_count} failed.")
    if not args.dry_run:
        print(f"Output files saved to: {out_dir.resolve()}")
    print("=" * 68)


if __name__ == "__main__":
    main()

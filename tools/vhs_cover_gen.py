"""Generate fake VHS covers using stock photos + typography templates.

Single mode (default):  python vhs_cover_gen.py
Batch mode:             python vhs_cover_gen.py --batch
"""

import os
import re
import sys
import math
import random
import hashlib
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from PIL import Image, ImageDraw, ImageFont, ImageOps, ImageFilter, ImageEnhance


def stable_hash(s):
    """Deterministic int hash, unlike Python's randomized hash()."""
    return int(hashlib.md5(s.encode("utf-8")).hexdigest()[:8], 16)

WIDTH, HEIGHT = 800, 1400
HERE = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(HERE, "generated_covers")
PHOTO_CACHE = os.path.join(HERE, "photo_cache")
EXTRA_FONTS_DIR = os.path.join(HERE, "fonts")
WIN_FONTS_DIR = "C:/Windows/Fonts"

# ---------------------------------------------------------------------------
# Free OFL fonts to fetch from Google Fonts' GitHub repo. These hit the
# VHS-cover aesthetic spots that Windows fonts don't cover (heavy display,
# condensed industrial, retro novelty, typewriter, neon).
# ---------------------------------------------------------------------------
GOOGLE_FONTS_BASE = "https://raw.githubusercontent.com/google/fonts/main"
EXTRA_FONTS = [
    # (filename, github path)
    ("BebasNeue-Regular.ttf", "ofl/bebasneue/BebasNeue-Regular.ttf"),
    ("Anton-Regular.ttf", "ofl/anton/Anton-Regular.ttf"),
    ("Oswald-Bold.ttf", "ofl/oswald/Oswald%5Bwght%5D.ttf"),
    ("Bungee-Regular.ttf", "ofl/bungee/Bungee-Regular.ttf"),
    ("BungeeInline-Regular.ttf", "ofl/bungeeinline/BungeeInline-Regular.ttf"),
    ("BungeeShade-Regular.ttf", "ofl/bungeeshade/BungeeShade-Regular.ttf"),
    ("BlackOpsOne-Regular.ttf", "ofl/blackopsone/BlackOpsOne-Regular.ttf"),
    ("Monoton-Regular.ttf", "ofl/monoton/Monoton-Regular.ttf"),
    ("PermanentMarker-Regular.ttf", "apache/permanentmarker/PermanentMarker-Regular.ttf"),
    ("SpecialElite-Regular.ttf", "apache/specialelite/SpecialElite-Regular.ttf"),
    ("AbrilFatface-Regular.ttf", "ofl/abrilfatface/AbrilFatface-Regular.ttf"),
    ("PlayfairDisplay-Bold.ttf", "ofl/playfairdisplay/PlayfairDisplay%5Bwght%5D.ttf"),
    ("Cinzel-Bold.ttf", "ofl/cinzel/Cinzel%5Bwght%5D.ttf"),
    ("RussoOne-Regular.ttf", "ofl/russoone/RussoOne-Regular.ttf"),
    ("SquadaOne-Regular.ttf", "ofl/squadaone/SquadaOne-Regular.ttf"),
    ("PaytoneOne-Regular.ttf", "ofl/paytoneone/PaytoneOne-Regular.ttf"),
    ("BowlbyOne-Regular.ttf", "ofl/bowlbyone/BowlbyOne-Regular.ttf"),
    ("Audiowide-Regular.ttf", "ofl/audiowide/Audiowide-Regular.ttf"),
    ("Limelight-Regular.ttf", "ofl/limelight/Limelight-Regular.ttf"),
    ("Megrim-Regular.ttf", "ofl/megrim/Megrim-Regular.ttf"),
    ("Codystar-Regular.ttf", "ofl/codystar/Codystar-Regular.ttf"),
    ("FasterOne-Regular.ttf", "ofl/fasterone/FasterOne-Regular.ttf"),
    ("Chango-Regular.ttf", "ofl/chango/Chango-Regular.ttf"),
    ("SixCaps.ttf", "ofl/sixcaps/SixCaps.ttf"),
    ("Megrim-Regular.ttf", "ofl/megrim/Megrim-Regular.ttf"),
    ("Yeseva-Bold.ttf", "ofl/yesevaone/YesevaOne-Regular.ttf"),
    ("RubikWetPaint-Regular.ttf", "ofl/rubikwetpaint/RubikWetPaint-Regular.ttf"),
    ("StintUltraExpanded-Regular.ttf", "ofl/stintultraexpanded/StintUltraExpanded-Regular.ttf"),
    ("Creepster-Regular.ttf", "ofl/creepster/Creepster-Regular.ttf"),
    ("NosiferRegular.ttf", "ofl/nosifer/Nosifer-Regular.ttf"),
]


def download_extra_fonts():
    os.makedirs(EXTRA_FONTS_DIR, exist_ok=True)
    fetched = 0
    for name, path in EXTRA_FONTS:
        out = os.path.join(EXTRA_FONTS_DIR, name)
        if os.path.exists(out) and os.path.getsize(out) > 5000:
            continue
        url = f"{GOOGLE_FONTS_BASE}/{path}"
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "vhs-cover-gen/1.0"})
            with urllib.request.urlopen(req, timeout=15) as r, open(out, "wb") as f:
                f.write(r.read())
            # Validate it loads
            ImageFont.truetype(out, 24)
            fetched += 1
        except Exception as e:
            print(f"  font fetch failed: {name}: {e}")
            if os.path.exists(out):
                os.remove(out)
    return fetched

# ---------------------------------------------------------------------------
# Distributor "house styles" — palette + font + accent choices.
# Drives most of the visual variety across the batch.
# ---------------------------------------------------------------------------
DISTRIBUTOR_STYLES = {
    "MIDNIGHT HOME VIDEO": {
        "title_pool": "pulp",
        "title_color": (245, 230, 200),
        "title_shadow": (140, 0, 0),
        "banner_color": (220, 60, 60),
        "tagline_color": (230, 200, 160),
        "tagline_font": "ariali.ttf",
        "rating_box_color": (255, 255, 255),
    },
    "STARDUST HOME ENTERTAINMENT": {
        "title_pool": "pulp",
        "title_color": (255, 220, 60),
        "title_shadow": (0, 0, 0),
        "banner_color": (255, 220, 60),
        "tagline_color": (255, 255, 255),
        "tagline_font": "ariali.ttf",
        "rating_box_color": (255, 220, 60),
    },
    "POLARIS PICTURES": {
        "title_pool": "pulp",
        "title_color": (180, 240, 255),
        "title_shadow": (40, 0, 110),
        "banner_color": (255, 80, 200),
        "tagline_color": (210, 230, 255),
        "tagline_font": "ariali.ttf",
        "rating_box_color": (255, 80, 200),
    },
    "VENTURA RELEASING": {
        "title_pool": "pulp",
        "title_color": (255, 255, 255),
        "title_shadow": (180, 130, 0),
        "banner_color": (220, 160, 40),
        "tagline_color": (255, 230, 180),
        "tagline_font": "ariali.ttf",
        "rating_box_color": (220, 160, 40),
    },
    "TRIDENT VIDEO": {
        "title_pool": "pulp",
        "title_color": (240, 240, 240),
        "title_shadow": (0, 60, 100),
        "banner_color": (60, 140, 200),
        "tagline_color": (200, 230, 255),
        "tagline_font": "ariali.ttf",
        "rating_box_color": (60, 140, 200),
    },
    "POWER PULSE FITNESS VIDEO": {
        "title_pool": "fitness",
        "title_color": (255, 255, 255),
        "title_shadow": (255, 50, 180),
        "banner_color": (255, 80, 200),
        "tagline_color": (255, 200, 240),
        "tagline_font": "ariali.ttf",
        "rating_box_color": (0, 220, 220),
    },
    "SUNSEEKER FITNESS VIDEO": {
        "title_pool": "fitness",
        "title_color": (255, 240, 80),
        "title_shadow": (255, 80, 0),
        "banner_color": (255, 100, 0),
        "tagline_color": (255, 240, 200),
        "tagline_font": "ariali.ttf",
        "rating_box_color": (255, 100, 0),
    },
    "RAINBOW HOME VIDEO": {
        "title_pool": "kids",
        "title_color": (255, 220, 60),
        "title_shadow": (200, 30, 30),
        "banner_color": (50, 180, 80),
        "tagline_color": (255, 255, 255),
        "tagline_font": "comic.ttf",
        "rating_box_color": (50, 130, 220),
    },
    "JOLLY KIDS VIDEO": {
        "title_pool": "kids",
        "title_color": (255, 100, 200),
        "title_shadow": (90, 30, 130),
        "banner_color": (255, 200, 60),
        "tagline_color": (255, 255, 255),
        "tagline_font": "comic.ttf",
        "rating_box_color": (50, 130, 220),
    },
    "SUNNYSIDE FAMILY VIDEO": {
        "title_pool": "kids",
        "title_color": (60, 150, 220),
        "title_shadow": (255, 220, 80),
        "banner_color": (255, 150, 50),
        "tagline_color": (255, 240, 200),
        "tagline_font": "comic.ttf",
        "rating_box_color": (255, 150, 50),
    },
    "HORIZON EDUCATIONAL VIDEO": {
        "title_pool": "serif",
        "title_color": (255, 245, 220),
        "title_shadow": (60, 30, 0),
        "banner_color": (160, 110, 50),
        "tagline_color": (240, 220, 180),
        "tagline_font": "timesi.ttf",
        "rating_box_color": (200, 170, 110),
    },
    "TRUE STORIES VIDEO": {
        "title_pool": "serif",
        "title_color": (240, 220, 180),
        "title_shadow": (40, 20, 10),
        "banner_color": (180, 50, 30),
        "tagline_color": (220, 200, 170),
        "tagline_font": "timesi.ttf",
        "rating_box_color": (180, 50, 30),
    },
    "SHEPHERD'S LIGHT MINISTRIES": {
        "title_pool": "serif",
        "title_color": (255, 240, 200),
        "title_shadow": (90, 60, 20),
        "banner_color": (200, 170, 90),
        "tagline_color": (240, 230, 200),
        "tagline_font": "timesi.ttf",
        "rating_box_color": (200, 170, 90),
    },
    "GOLDEN GATE FAITH VIDEO": {
        "title_pool": "serif",
        "title_color": (250, 230, 150),
        "title_shadow": (110, 70, 20),
        "banner_color": (220, 180, 80),
        "tagline_color": (255, 240, 200),
        "tagline_font": "timesi.ttf",
        "rating_box_color": (220, 180, 80),
    },
}

DEFAULT_STYLE_KEY = "MIDNIGHT HOME VIDEO"

# ---------------------------------------------------------------------------
# 100 fake titles. (title, tagline, genre_banner, distributor, rating, runtime)
# Picsum will provide a deterministic random photo per title for layout test.
# Real photos can be dropped in per-cover later.
# ---------------------------------------------------------------------------
COVERS = [
    # ------- THRILLER --------
    ("HOLLOWPOINT", "One round.\nOne name.\nOne night.", "TERROR DOWNTOWN", "MIDNIGHT HOME VIDEO", "R", "89"),
    ("DEAD AIR", "The radio host who couldn't go home.", "A NIGHT IN HER OWN STATION", "VENTURA RELEASING", "R", "94"),
    ("THE KEY HOLDER", "Every door has a price.", "KEY OF MENACE", "MIDNIGHT HOME VIDEO", "R", "91"),
    ("MIDNIGHT COMMUTER", "The 3 AM train doesn't stop for anyone.", "STRANGER ON THE LINE", "TRIDENT VIDEO", "R", "88"),
    ("BACKROAD", "He took the shortcut.\nHe never got home.", "A WRONG TURN INTO TERROR", "MIDNIGHT HOME VIDEO", "R", "95"),
    ("THE SUBSTITUTE TEACHER", "Class is in session.", "SHE GRADES IN BLOOD", "VENTURA RELEASING", "R", "90"),
    ("COLD STORAGE", "Some things should stay frozen.", "THE FREEZER WON'T LOCK", "MIDNIGHT HOME VIDEO", "R", "87"),
    ("NIGHT WATCH", "He walks the halls at midnight.", "A GUARD'S DARKEST ROUNDS", "TRIDENT VIDEO", "R", "92"),
    ("SOFT TARGET", "She thought he was harmless.\nShe was wrong.", "A TENANT'S FATAL TRUST", "VENTURA RELEASING", "R", "93"),
    ("THE INSURANCE CLAIM", "Some accidents aren't accidents.", "A POLICY OF MURDER", "MIDNIGHT HOME VIDEO", "R", "96"),
    ("THE LIGHTHOUSE TENANT", "He came for solitude.\nHe found something else.", "TERROR BY THE SEA", "TRIDENT VIDEO", "R", "94"),
    ("MOTEL CALIFORNIA", "Rooms by the hour.\nMemories forever.", "VACANCY OF THE DAMNED", "MIDNIGHT HOME VIDEO", "R", "89"),
    ("THE NIGHT VALET", "He parked the cars.\nHe buried the drivers.", "TIPS ARE NOT NEGOTIABLE", "VENTURA RELEASING", "R", "86"),
    ("ROUTE 9 KILLER", "She drives a Buick.\nShe drives to kill.", "A HIGHWAY OF FEAR", "MIDNIGHT HOME VIDEO", "R", "91"),
    ("THE EXTERMINATOR'S WIFE", "He ate his work home.\nShe acquired a taste.", "DOMESTIC HORROR", "TRIDENT VIDEO", "R", "88"),
    ("SLOW BURN", "The fire department was already too late.", "ARSON OF THE HEART", "VENTURA RELEASING", "R", "94"),
    ("THE HOUSE GUEST", "He brought a gift.\nHe left a body.", "UNEXPECTED ARRIVALS", "MIDNIGHT HOME VIDEO", "R", "92"),
    ("THE NIGHT CASHIER", "11:00 PM. Last shift.\nLast breath.", "TERROR AT THE REGISTER", "VENTURA RELEASING", "R", "85"),
    ("THE FOSTER MOTHER", "She raised them all.\nShe raised them wrong.", "A FAMILY OF FEAR", "MIDNIGHT HOME VIDEO", "R", "97"),
    ("THE PARKING ATTENDANT", "Your keys.\nYour car.\nYour life.", "VALET OF DEATH", "TRIDENT VIDEO", "R", "88"),
    ("ENGINE OUT", "Stranded miles from anywhere.\nWith company.", "A ROADSIDE NIGHTMARE", "MIDNIGHT HOME VIDEO", "R", "90"),
    ("THE SLEEP CLINIC", "You can't wake from this dream.", "TERROR IN STAGE 4", "VENTURA RELEASING", "R", "93"),
    ("ROOM SERVICE", "Order anything.\nPay everything.", "HOTEL OF HORRORS", "MIDNIGHT HOME VIDEO", "R", "91"),
    ("PROPERTY LINE", "The fence didn't hold him.", "A NEIGHBOR'S NIGHTMARE", "TRIDENT VIDEO", "R", "89"),
    ("THE NIGHT NURSE", "She works the late shift.\nPermanently.", "IV OF TERROR", "VENTURA RELEASING", "R", "94"),

    # ------- HORROR --------
    ("THE BASEMENT TAPES", "Don't watch them.", "RECORDED IN BLOOD", "MIDNIGHT HOME VIDEO", "R", "87"),
    ("ATTIC CRAWL", "Something's been up there a long time.", "DON'T LOOK UP", "TRIDENT VIDEO", "R", "88"),
    ("THE LISTENING ROOM", "The walls remember every word.", "TERROR IN STEREO", "MIDNIGHT HOME VIDEO", "R", "92"),
    ("SHADOWFEED", "The cable never gets installed.", "STATIC OF THE DAMNED", "POLARIS PICTURES", "R", "90"),
    ("SPLIT TONGUE", "Two voices.\nOne throat.", "POSSESSED!", "MIDNIGHT HOME VIDEO", "R", "94"),
    ("THE WHISPERING WELL", "Don't drink.\nDon't listen.", "A VILLAGE OF DREAD", "VENTURA RELEASING", "R", "91"),
    ("ROOM 14", "Other guests have stayed.\nNone checked out.", "UNRESERVED HORROR", "MIDNIGHT HOME VIDEO", "R", "89"),
    ("BURIAL HYMN", "The choir sang.\nThe dead answered.", "A REQUIEM OF EVIL", "TRIDENT VIDEO", "R", "97"),
    ("THE NEIGHBOR", "She bakes.\nShe knocks.\nShe waits.", "DOMESTIC TERROR", "MIDNIGHT HOME VIDEO", "R", "88"),
    ("STATIC", "Channel 13 doesn't exist.\nUntil tonight.", "SIGNAL FROM HELL", "POLARIS PICTURES", "R", "85"),
    ("THE EXTERMINATOR", "He came for the rats.\nHe stayed for everything else.", "INFESTATION", "MIDNIGHT HOME VIDEO", "R", "92"),
    ("NIGHT DELIVERY", "The package wasn't food.", "A COURIER'S CURSE", "VENTURA RELEASING", "R", "86"),
    ("CRAWLSPACE", "Something's been living under the house.", "DON'T LIFT THE BOARDS", "MIDNIGHT HOME VIDEO", "R", "90"),
    ("THE CARETAKER", "He keeps the grounds.\nHe keeps the bodies.", "GROUNDS OF GRAVES", "TRIDENT VIDEO", "R", "94"),
    ("THE MANNEQUIN", "The store closed.\nThey didn't.", "RETAIL OF THE DAMNED", "MIDNIGHT HOME VIDEO", "R", "87"),

    # ------- ACTION --------
    ("BLOOD DEBT", "He owes the wrong people.\nThey collect.", "A CONTRACT OF VENGEANCE", "VENTURA RELEASING", "R", "96"),
    ("IRON HEEL", "One man.\nOne boot.\nOne war.", "ONE-MAN ARMY", "STARDUST HOME ENTERTAINMENT", "R", "92"),
    ("THE CONTRACT", "$50,000 down.\nSoul not included.", "A KILLER FOR HIRE", "VENTURA RELEASING", "R", "94"),
    ("SAIGON FALLOUT", "The war ended.\nHis didn't.", "VETERAN OF VENGEANCE", "STARDUST HOME ENTERTAINMENT", "R", "98"),
    ("HARDLINE", "Cross the line.\nPay the price.", "A COP UNCHAINED", "VENTURA RELEASING", "R", "91"),
    ("KILL CIRCUIT", "Nine fights.\nOne survivor.", "THE TOURNAMENT BEGINS", "STARDUST HOME ENTERTAINMENT", "R", "89"),
    ("DESERT JUSTICE", "No badge.\nNo backup.\nNo mercy.", "A LAWMAN'S LAST RIDE", "VENTURA RELEASING", "R", "93"),
    ("THE LAST MISSION", "He retired.\nThe enemy didn't get the memo.", "VETERAN UNLEASHED", "STARDUST HOME ENTERTAINMENT", "R", "95"),
    ("COMBAT ZONE", "The city is the war.", "URBAN WARFARE", "VENTURA RELEASING", "R", "88"),
    ("WHITE PHOSPHORUS", "Burn the bridge.\nBurn the bodies.", "A SOLDIER OF FIRE", "STARDUST HOME ENTERTAINMENT", "R", "92"),
    ("RED ZONE", "Cross the border.\nCross your life off.", "BORDER WARFARE", "VENTURA RELEASING", "R", "91"),
    ("PROJECT REDLINE", "The experiment escaped.\nIt's armed.", "TERROR FROM THE LAB", "POLARIS PICTURES", "R", "89"),

    # ------- NINJA / MARTIAL --------
    ("NINJA BLOODBATH", "One thousand cuts.\nOne target.", "AN ASSASSIN'S CODE", "STARDUST HOME ENTERTAINMENT", "R", "86"),
    ("KARATE EXILE", "Banished from his school.\nHunted by his master.", "A WARRIOR'S RETURN", "VENTURA RELEASING", "R", "90"),
    ("FIST OF SHANGHAI", "He fights for honor.\nHe fights to live.", "KUNG FU FURY", "STARDUST HOME ENTERTAINMENT", "R", "88"),
    ("DRAGON LAW", "Old code.\nNew blood.", "A SAMURAI'S REVENGE", "STARDUST HOME ENTERTAINMENT", "R", "92"),
    ("SILENT KICK", "He doesn't speak.\nHis feet do.", "DEADLY SILENCE", "VENTURA RELEASING", "R", "85"),
    ("THE WHITE TIGER", "She trained in shadows.\nShe fights in the open.", "FEMALE FURY", "STARDUST HOME ENTERTAINMENT", "R", "89"),
    ("KUMITE KING", "Eight rounds.\nSeven dead.\nOne champion.", "UNDERGROUND TOURNAMENT", "STARDUST HOME ENTERTAINMENT", "R", "91"),
    ("IRON DRAGON", "His fists are weapons.\nHis past is a wound.", "A MARTIAL MASTERPIECE", "VENTURA RELEASING", "R", "93"),

    # ------- SCI-FI --------
    ("GRAVITY DEBT", "He owes the planet a body.", "DEEP SPACE TERROR", "POLARIS PICTURES", "PG", "94"),
    ("COSMIC TRESPASS", "He landed uninvited.\nHe won't leave.", "ALIEN INTRUDER", "POLARIS PICTURES", "R", "97"),
    ("THE PARALLAX MAN", "He sees through walls.\nHe sees through time.", "A MIND IN ORBIT", "POLARIS PICTURES", "PG", "88"),
    ("NEUTRINO", "The particle that ends the world.", "SCIENCE GONE WRONG", "POLARIS PICTURES", "PG", "92"),
    ("ORBIT 7", "Seven astronauts.\nSix suits.\nOne survivor.", "A STATION OF SECRETS", "POLARIS PICTURES", "PG", "95"),
    ("DEEP STATIC", "The signal came from inside the hull.", "A SHIP OF GHOSTS", "POLARIS PICTURES", "R", "91"),
    ("HOLOGRAM CITY", "The skyline lies.\nThe killers don't.", "A DIGITAL DETECTIVE", "POLARIS PICTURES", "R", "93"),
    ("CYBER COURIER", "Delivery in 24 hours.\nDeath in 12.", "A MESSENGER OF THE FUTURE", "POLARIS PICTURES", "PG", "89"),

    # ------- KIDS / FAMILY --------
    ("JINGLE & JANGLE'S BIRTHDAY", "Fun, friends, and a giant cake!", "FUN FOR ALL AGES!", "JOLLY KIDS VIDEO", "G", "42"),
    ("THE TALKING DOG NEXT DOOR", "Woof means hello!", "A FURRY ADVENTURE!", "SUNNYSIDE FAMILY VIDEO", "G", "55"),
    ("PUDDING PALACE", "A castle made of sweets!", "A YUMMY ADVENTURE!", "RAINBOW HOME VIDEO", "G", "38"),
    ("ROBBIE THE RECYCLING ROBOT", "Help save Planet Earth!", "GO GREEN WITH ROBBIE!", "JOLLY KIDS VIDEO", "G", "44"),
    ("CAPTAIN BUBBLE", "He flies! He floats! He pops!", "BUBBLE-RIFFIC FUN!", "RAINBOW HOME VIDEO", "G", "40"),
    ("DINOSAUR FARM", "Where the chores are giant!", "PREHISTORIC PALS!", "SUNNYSIDE FAMILY VIDEO", "G", "48"),
    ("THE LITTLE PILGRIMS", "A Thanksgiving they'll never forget!", "A HARVEST OF FUN!", "JOLLY KIDS VIDEO", "G", "52"),
    ("MAGIC MEADOW", "Where flowers sing\nand clouds dance!", "A MUSICAL MEADOW!", "RAINBOW HOME VIDEO", "G", "45"),
    ("ZOO FRIENDS FOREVER", "Meet your new best pals!", "A WHOLE ZOO OF FUN!", "SUNNYSIDE FAMILY VIDEO", "G", "50"),
    ("THE GRUMPY PUMPKIN", "He just needs a smile!", "A HAUNTED-LIGHT TALE!", "JOLLY KIDS VIDEO", "G", "33"),
    ("SAFETY SCOUT SAYS", "Look both ways!", "FUN AND SAFE!", "SUNNYSIDE FAMILY VIDEO", "G", "29"),
    ("THE ALPHABET ZOO", "26 letters,\n26 animals!", "LEARN AND PLAY!", "RAINBOW HOME VIDEO", "G", "60"),

    # ------- FITNESS --------
    ("AEROBIC ATTACK", "20 minutes. Total body.\nMaximum heat.", "FEEL THE BURN!", "POWER PULSE FITNESS VIDEO", "NR", "55"),
    ("SLIM FOR SUMMER", "Beach body in 14 days!", "RESULTS GUARANTEED!", "SUNSEEKER FITNESS VIDEO", "NR", "50"),
    ("POWER STRETCH", "Lengthen, lean, transform.", "DAILY FLEXIBILITY!", "POWER PULSE FITNESS VIDEO", "NR", "45"),
    ("THE 30-DAY TRANSFORMATION", "A new you in one month!", "AS SEEN ON TV!", "SUNSEEKER FITNESS VIDEO", "NR", "60"),
    ("DANCE OFF THE POUNDS", "The hottest moves.\nThe quickest results.", "DANCE YOUR WAY THIN!", "POWER PULSE FITNESS VIDEO", "NR", "52"),
    ("RHYTHM SCULPT", "Dance your way to tone.", "THE WORKOUT REVOLUTION!", "POWER PULSE FITNESS VIDEO", "NR", "48"),
    ("LEAN MACHINE", "30 minutes a day.\nTotal body sculpt.", "TRANSFORM TODAY!", "SUNSEEKER FITNESS VIDEO", "NR", "35"),
    ("BODY HEAT WORKOUT", "Sweat. Burn. Repeat.", "THE HOTTEST HOUR ON TAPE!", "SUNSEEKER FITNESS VIDEO", "NR", "60"),

    # ------- DOCUMENTARY / EDUCATIONAL / RELIGIOUS --------
    ("WONDERS OF THE AMAZON", "A journey into the world's last frontier.", "AN EDUCATIONAL JOURNEY", "HORIZON EDUCATIONAL VIDEO", "NR", "58"),
    ("SECRETS OF THE PHARAOHS", "What lies beneath the sands of Egypt?", "ANCIENT MYSTERIES REVEALED", "TRUE STORIES VIDEO", "NR", "60"),
    ("AMERICA'S HAUNTED HIGHWAYS", "The roads that remember.", "TRUE STORIES OF THE PARANORMAL", "TRUE STORIES VIDEO", "NR", "55"),
    ("INSIDE THE VOLCANO", "The earth's molten heart.", "NATURE'S FURY UP CLOSE", "HORIZON EDUCATIONAL VIDEO", "NR", "52"),
    ("THE MIND OF A SHARK", "Predator. Hunter. Survivor.", "AN OCEAN PORTRAIT", "HORIZON EDUCATIONAL VIDEO", "NR", "56"),
    ("WALK WITH HIM", "A devotional journey.", "INSPIRATIONAL VIEWING", "SHEPHERD'S LIGHT MINISTRIES", "NR", "60"),
    ("THE LAMP UNTO MY FEET", "Scripture, song, and stillness.", "DAILY DEVOTION", "SHEPHERD'S LIGHT MINISTRIES", "NR", "45"),
    ("SUNDAY MORNING MIRACLES", "Stories of faith and hope.", "TESTIMONY ON TAPE", "GOLDEN GATE FAITH VIDEO", "NR", "55"),
    ("THE LITTLE PARABLE", "Bible stories for young hearts.", "FAMILY FAITH VIEWING", "GOLDEN GATE FAITH VIDEO", "G", "40"),
    ("BIRDS OF NORTH AMERICA", "100 species. One unforgettable hour.", "AUDUBON ON VIDEO", "HORIZON EDUCATIONAL VIDEO", "NR", "60"),
    ("THE BIGFOOT FILES", "Witnesses speak.\nCameras roll.", "UNEXPLAINED ENCOUNTERS", "TRUE STORIES VIDEO", "NR", "58"),
    ("PLANET EARTH UNCOVERED", "A documentary in three parts.", "EDUCATIONAL EPIC", "HORIZON EDUCATIONAL VIDEO", "NR", "60"),
]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def slug(s):
    s = re.sub(r"[^A-Za-z0-9]+", "_", s).strip("_").lower()
    return s[:80]


def _resolve_font_path(name):
    """Search extra fonts dir, system fonts, then bare name (PIL system lookup)."""
    for d in (EXTRA_FONTS_DIR, WIN_FONTS_DIR):
        p = os.path.join(d, name)
        if os.path.exists(p):
            return p
    return name


def find_font(name, size):
    for candidate in (_resolve_font_path(name), name.lower()):
        try:
            return ImageFont.truetype(candidate, size)
        except (OSError, IOError):
            continue
    return ImageFont.load_default()


# ---------------------------------------------------------------------------
# Font pools — per-genre lists of title fonts. Picked per-title via hash so
# every cover gets a different font even within the same distributor.
# ---------------------------------------------------------------------------
RAW_TITLE_POOLS = {
    # The "pulp" pool is split into sub-pools so every cover doesn't default
    # to bold-condensed-sans. pick_font dispatches to one of these by hash.
    "pulp_bold": [
        "impact.ttf", "ariblk.ttf", "arialbd.ttf", "bahnschrift.ttf",
        "segoeuib.ttf", "seguibl.ttf", "tahomabd.ttf", "verdanab.ttf",
        "framd.ttf", "trebucbd.ttf",
        "BebasNeue-Regular.ttf", "Anton-Regular.ttf", "Oswald-Bold.ttf",
        "RussoOne-Regular.ttf", "SquadaOne-Regular.ttf", "SixCaps.ttf",
        "StintUltraExpanded-Regular.ttf",
    ],
    "pulp_novelty": [
        # 80s/90s display weirdness — strong silhouettes, instantly distinctive
        "Bungee-Regular.ttf", "BungeeInline-Regular.ttf", "BungeeShade-Regular.ttf",
        "Audiowide-Regular.ttf", "FasterOne-Regular.ttf", "Codystar-Regular.ttf",
        "Monoton-Regular.ttf", "BlackOpsOne-Regular.ttf", "Chango-Regular.ttf",
    ],
    "pulp_horror": [
        # Drippy / creepy / paint stroke
        "Creepster-Regular.ttf", "NosiferRegular.ttf",
        "RubikWetPaint-Regular.ttf",
    ],
    "pulp_retro": [
        # Vintage display + heavy serif + handwritten — Hollywood/golden-age feel
        "AbrilFatface-Regular.ttf", "Yeseva-Bold.ttf", "PlayfairDisplay-Bold.ttf",
        "Limelight-Regular.ttf", "PaytoneOne-Regular.ttf", "BowlbyOne-Regular.ttf",
        "PermanentMarker-Regular.ttf", "SpecialElite-Regular.ttf",
        "timesbd.ttf", "georgiab.ttf", "cambriab.ttf", "palab.ttf",
    ],
    "kids": [
        "comicbd.ttf", "inkfree.ttf", "segoescb.ttf", "segoeprb.ttf",
        "gabriola.ttf", "corbelb.ttf", "candarab.ttf", "framd.ttf",
        "PermanentMarker-Regular.ttf", "Bungee-Regular.ttf",
        "BungeeInline-Regular.ttf", "BowlbyOne-Regular.ttf",
        "PaytoneOne-Regular.ttf", "Chango-Regular.ttf",
        "AbrilFatface-Regular.ttf", "RubikWetPaint-Regular.ttf",
    ],
    "fitness": [
        "impact.ttf", "ariblk.ttf", "bahnschrift.ttf", "seguibl.ttf",
        "arialbi.ttf", "verdanaz.ttf", "segoeuiz.ttf", "tahomabd.ttf",
        "BebasNeue-Regular.ttf", "Anton-Regular.ttf", "Oswald-Bold.ttf",
        "BlackOpsOne-Regular.ttf", "RussoOne-Regular.ttf",
        "Audiowide-Regular.ttf", "SquadaOne-Regular.ttf",
        "BungeeInline-Regular.ttf",
    ],
    "serif": [
        "timesbd.ttf", "timesbi.ttf", "georgiab.ttf", "georgiaz.ttf",
        "cambriab.ttf", "cambriaz.ttf", "constanb.ttf", "palab.ttf",
        "palabi.ttf", "sylfaen.ttf",
        "AbrilFatface-Regular.ttf", "PlayfairDisplay-Bold.ttf",
        "Cinzel-Bold.ttf", "Limelight-Regular.ttf", "Yeseva-Bold.ttf",
        "SpecialElite-Regular.ttf",
    ],
}

RAW_TAGLINE_POOL = [
    "ariali.ttf", "arialbi.ttf", "timesi.ttf", "timesbi.ttf",
    "georgiai.ttf", "georgiaz.ttf", "calibrii.ttf", "calibrili.ttf",
    "candarai.ttf", "candarali.ttf", "corbeli.ttf", "corbelli.ttf",
    "cambriai.ttf", "constani.ttf", "palai.ttf", "palabi.ttf",
    "segoeuii.ttf", "seguili.ttf", "seguisli.ttf", "seguisbi.ttf",
    "segoesc.ttf", "segoescb.ttf", "segoepr.ttf", "segoeprb.ttf",
    "mvboli.ttf", "trebucit.ttf", "trebucbi.ttf",
    "verdanai.ttf", "verdanaz.ttf", "ebrima.ttf",
    "inkfree.ttf", "gabriola.ttf",
    "SpecialElite-Regular.ttf", "PermanentMarker-Regular.ttf",
]


def _validate_pool(names):
    valid = []
    for n in names:
        try:
            ImageFont.truetype(_resolve_font_path(n), 24)
            valid.append(n)
        except (OSError, IOError):
            pass
    return valid


TITLE_POOLS = {k: _validate_pool(v) for k, v in RAW_TITLE_POOLS.items()}
TAGLINE_POOL = _validate_pool(RAW_TAGLINE_POOL)


PULP_SUBPOOLS = ["pulp_bold", "pulp_novelty", "pulp_horror", "pulp_retro"]


def pick_font(pool, seed_str, fallback="arialbd.ttf"):
    """Pick a font filename from a pool deterministically from a seed string.

    For 'pulp' the call routes through 4 sub-pools (bold/novelty/horror/retro)
    so distinctive fonts aren't drowned out by bold-condensed-sans.
    """
    if isinstance(pool, str):
        if pool == "pulp":
            sub = PULP_SUBPOOLS[stable_hash(seed_str + ":sub") % len(PULP_SUBPOOLS)]
            pool = TITLE_POOLS.get(sub) or TITLE_POOLS.get("pulp_bold") or [fallback]
        else:
            pool = TITLE_POOLS.get(pool) or [fallback]
    if not pool:
        return fallback
    return pool[stable_hash(seed_str) % len(pool)]


def report_pool_sizes():
    print("Title pools:", {k: len(v) for k, v in TITLE_POOLS.items()})
    print(f"Tagline pool: {len(TAGLINE_POOL)} fonts")


# ---------------------------------------------------------------------------
# VHS-specific text content — actor credits, critic blurbs, bursts, tech strips.
# Picked deterministically per title so a cover's metadata is stable across runs.
# ---------------------------------------------------------------------------
ACTOR_NAMES = [
    "ROBERT VANCE", "LISA STERN", "ANDY CRIGHT", "KENT LASKER",
    "DIANA PRESCOTT", "TONY ROSARIO", "MICHAEL HUTCHENS", "GAIL KOEHLER",
    "DAVID FORTNEY", "TRACE BARRINGTON", "VINCE MORENO", "KAREN WHITMER",
    "STEVE ALDRIDGE", "JANET BOWERS", "DALE MCCAFFREY", "SHARI VOORHEES",
    "RICK CONRADO", "MELODY KENT", "PAUL HENNESSY", "TINA SAVAGE",
    "GLEN BORMAN", "CHRIS DELANEY", "SUE ALMSTROM", "MARK FUENTES",
    "JIM HALLORAN", "CLAIRE WAINWRIGHT", "NICK CASSARA", "DEBRA MOFFAT",
    "RYAN HOLLINGER", "CAROL ESCOBEDO", "BRAD STIVERS", "AMY VICKERS",
    "RAY BRENNAN", "JOAN TIPTON", "WAYNE COFFIN", "BRENDA STARK",
    "EARL TANAKA", "MIKE SOTOMAYOR", "PATTI VAN HORN", "TED CALLOWAY",
    "SUSAN KRAUSE", "GARRY WALDORF", "LINDA OSGOOD", "KEN PINKERTON",
    "BARBARA EDISON", "LEE FINKBINER", "MARY DELAHUNT", "GEORGE NESBIT",
    "HOLLY CRENSHAW", "JOEY BUCKHALTER", "LANA BURGOS", "CHET YEAMAN",
    "DENISE BRONSON", "ALEX SANCHEZ", "LORI WIGGINS", "PETE NEMEC",
    "RHONDA FRAZIER", "CAL BONIN", "SUZY WIDMARK", "MAX ARTUSO",
    "TAMMY HORST", "JOEL BERNARDI", "TRACY MEDINA", "SCOTT NIELSON",
    "DEANNA RIVAS", "BO HARGRAVE", "SHEILA LATTIMORE", "LARRY DOUGAN",
    "JEANNE HOOPER", "ROY VESELKA", "PAM REINER", "KIRK BRAUN",
    "JANE DOROSHENKO", "CHUCK STRAWBRIDGE", "MARY SUE FORD", "TODD MONFORT",
    "NORMA STEINBECK", "DUKE PRATER", "ANGIE LEFEBVRE", "RUSS BOULDIN",
]

CRITIC_BLURBS = [
    "EXPLOSIVE!", "PULSE-POUNDING!", "WICKEDLY ENTERTAINING!",
    "EDGE-OF-YOUR-SEAT!", "A NEW CLASSIC!", "DON'T WATCH ALONE!",
    "JAW-DROPPING!", "BRUTAL!", "BRILLIANT!", "A KNOCKOUT!",
    "RIVETING!", "TERRIFYING!", "UNFORGETTABLE!", "MASTERFUL!",
    "A WILD RIDE!", "POWERFUL!", "HEART-STOPPING!", "GLORIOUS!",
    "INTENSE!", "GRIPPING!", "SHOCKING!", "WHITE-KNUCKLE!",
    "MESMERIZING!", "VISCERAL!", "A TRIUMPH!", "DELIRIOUS FUN!",
    "WILDLY ORIGINAL!", "WHITE-HOT!", "STUNNING!", "OUTRAGEOUS!",
]

CRITIC_SOURCES = [
    "VIDEO REVIEW", "TV GUIDE", "TIME OUT", "ENTERTAINMENT WEEKLY",
    "VARIETY", "THE HOLLYWOOD TIMES", "VIDEO STORE NEWS",
    "VIDEO BUSINESS", "ROLLING STONE", "HBO MAGAZINE",
    "CABLE GUIDE", "L.A. WEEKLY", "NEW YORK POST",
    "FILM THREAT", "VIDEOWAVE",
]

BURST_TEXTS = [
    "NEW RELEASE!", "COLLECTOR'S EDITION!", "WIDESCREEN!", "UNRATED!",
    "DIRECTOR'S CUT!", "DOUBLE FEATURE!", "SPECIAL EDITION!",
    "TWO-TAPE SET!", "MUST OWN!", "INSTANT CLASSIC!", "UNCUT!",
    "REMASTERED!", "HIT MOVIE!", "AS SEEN IN THEATERS!",
]

TECH_STRIPS = [
    "HI-FI STEREO  •  CLOSED CAPTIONED  •  COLOR",
    "DOLBY SURROUND  •  CC  •  FULL COLOR",
    "HI-FI STEREO  •  CC  •  COLOR  •  APPROX. {min} MIN.",
    "DIGITALLY MASTERED  •  STEREO  •  COLOR",
    "FULL SCREEN PRESENTATION  •  STEREO  •  COLOR",
]


def format_starring(title):
    """Three actor names separated by bullets."""
    seeds = [stable_hash(title + f":star{i}") % len(ACTOR_NAMES) for i in range(3)]
    # Avoid duplicates
    picks = []
    for s in seeds:
        n = ACTOR_NAMES[s]
        offset = 0
        while n in picks:
            offset += 1
            n = ACTOR_NAMES[(s + offset) % len(ACTOR_NAMES)]
        picks.append(n)
    return "STARRING  " + "  •  ".join(picks)


def format_blurb(title):
    blurb = CRITIC_BLURBS[stable_hash(title + ":blurb") % len(CRITIC_BLURBS)]
    source = CRITIC_SOURCES[stable_hash(title + ":source") % len(CRITIC_SOURCES)]
    star_count = [3.0, 3.5, 4.0, 4.0][stable_hash(title + ":stars") % 4]
    full_stars = int(star_count)
    half = star_count - full_stars >= 0.5
    return full_stars, half, f'"{blurb}"', f"— {source}"


def draw_star_polygon(draw, cx, cy, r, color, half=False):
    """5-point star drawn as a polygon. half=True draws only the left half (filled),
    right half outlined — represents a half-star rating."""
    pts = []
    for i in range(10):
        ang = math.pi * (i / 5 - 0.5)
        rr = r if i % 2 == 0 else r * 0.42
        pts.append((cx + rr * math.cos(ang), cy + rr * math.sin(ang)))
    if not half:
        draw.polygon(pts, fill=color)
    else:
        # outline the full star, then fill only the left half
        draw.polygon(pts, outline=color)
        # Clip-fill via overlay would be cleanest; quick version: fill polygon
        # of left-side points only
        left_pts = [(min(x, cx), y) for (x, y) in pts]
        draw.polygon(left_pts, fill=color)


def draw_stars_row(draw, full, half, x, y, size, color):
    """Returns the total width the row took."""
    gap = 4
    r = size / 2
    cx = x + r
    drawn = 0
    for _ in range(full):
        draw_star_polygon(draw, cx, y + r, r, color)
        cx += size + gap
        drawn += size + gap
    if half:
        draw_star_polygon(draw, cx, y + r, r, color, half=True)
        drawn += size + gap
    return drawn - gap if drawn else 0


def has_burst(title):
    return stable_hash(title + ":hasburst") % 100 < 45


def burst_text(title):
    return BURST_TEXTS[stable_hash(title + ":burst") % len(BURST_TEXTS)]


def has_blurb(title):
    return stable_hash(title + ":hasblurb") % 100 < 60


def tech_strip(title, runtime):
    template = TECH_STRIPS[stable_hash(title + ":tech") % len(TECH_STRIPS)]
    return template.replace("{min}", str(runtime))


def presents_line(distributor):
    return f"{distributor}  PRESENTS"


# ---------------------------------------------------------------------------
# Drawing helpers for the new VHS elements
# ---------------------------------------------------------------------------
def draw_starring(draw, title, x, y, max_width, color, align="center"):
    text = format_starring(title)
    # Pick a small condensed sans for credits
    font = find_font("arialbd.ttf", size=18)
    bbox = draw.textbbox((0, 0), text, font=font)
    tw = bbox[2] - bbox[0]
    # Shrink if too wide
    size = 18
    while tw > max_width and size > 12:
        size -= 1
        font = find_font("arialbd.ttf", size=size)
        bbox = draw.textbbox((0, 0), text, font=font)
        tw = bbox[2] - bbox[0]
    if align == "center":
        draw.text((x + (max_width - tw) // 2, y), text, font=font, fill=color)
    elif align == "right":
        draw.text((x + max_width - tw, y), text, font=font, fill=color)
    else:
        draw.text((x, y), text, font=font, fill=color)
    return bbox[3] - bbox[1]


def draw_critic_blurb(draw, title, x, y, max_width, color, align="center"):
    """Stars drawn as polygons (font-independent), blurb in random italic font."""
    full_stars, half_star, blurb, source = format_blurb(title)
    blurb_font = find_font(pick_font(TAGLINE_POOL, title + ":blurbfont"), size=24)
    source_font = find_font("arialbd.ttf", size=14)
    star_size = 22
    star_gap = 4
    n_marks = full_stars + (1 if half_star else 0)
    stars_w = n_marks * star_size + (n_marks - 1) * star_gap if n_marks else 0
    sep_w = 12

    sb_blurb = draw.textbbox((0, 0), blurb, font=blurb_font)
    sb_source = draw.textbbox((0, 0), source, font=source_font)
    blurb_w = sb_blurb[2] - sb_blurb[0]
    source_w = sb_source[2] - sb_source[0]
    line1_w = stars_w + sep_w + blurb_w
    h1 = max(star_size, sb_blurb[3] - sb_blurb[1])
    h2 = sb_source[3] - sb_source[1]

    if align == "center":
        x0 = x + (max_width - line1_w) // 2
        x_src = x + (max_width - source_w) // 2
    elif align == "right":
        x0 = x + max_width - line1_w
        x_src = x + max_width - source_w
    else:
        x0 = x
        x_src = x

    # Stars (polygon, top-aligned with blurb baseline)
    star_y = y + (h1 - star_size) // 2
    actual_stars_w = draw_stars_row(draw, full_stars, half_star, x0, star_y, star_size, color)
    # Blurb italic, vertically centered with stars
    blurb_y = y + (h1 - (sb_blurb[3] - sb_blurb[1])) // 2
    draw.text((x0 + actual_stars_w + sep_w, blurb_y), blurb, font=blurb_font, fill=color)
    # Source
    draw.text((x_src, y + h1 + 6), source, font=source_font, fill=color)
    return h1 + h2 + 6


def draw_burst_sticker(canvas, text, cx, cy, fill_color, text_color, angle=-18, radius=110):
    """Diagonal starburst sticker with text. Drawn on a transparent overlay then composited."""
    layer_w = layer_h = radius * 3
    layer = Image.new("RGBA", (layer_w, layer_h), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    # 12-point starburst polygon
    points = []
    cx2, cy2 = layer_w // 2, layer_h // 2
    n_points = 18
    for i in range(n_points * 2):
        r = radius if i % 2 == 0 else int(radius * 0.78)
        ang = i * math.pi / n_points
        points.append((cx2 + r * math.cos(ang), cy2 + r * math.sin(ang)))
    ld.polygon(points, fill=fill_color)
    # Text inside, shrink to fit
    size = 22
    font = find_font("ariblk.ttf", size=size)
    text_lines = text.split(" ", 1) if len(text) > 12 else [text]
    while size > 10:
        widths = [ld.textbbox((0, 0), line, font=font)[2] for line in text_lines]
        total_h = len(text_lines) * (size + 4)
        if max(widths) < radius * 1.5 and total_h < radius * 1.4:
            break
        size -= 2
        font = find_font("ariblk.ttf", size=size)
    line_h = size + 4
    total_h = len(text_lines) * line_h
    y0 = cy2 - total_h // 2
    for i, line in enumerate(text_lines):
        bbox = ld.textbbox((0, 0), line, font=font)
        lw = bbox[2] - bbox[0]
        ld.text((cx2 - lw // 2, y0 + i * line_h - 4), line, font=font, fill=text_color)

    layer = layer.rotate(angle, resample=Image.BICUBIC, expand=False)
    canvas_rgba = canvas.convert("RGBA")
    canvas_rgba.alpha_composite(layer, (cx - layer_w // 2, cy - layer_h // 2))
    return canvas_rgba.convert("RGB")


def make_placeholder_bg(w, h, seed=7):
    rng = random.Random(seed)
    img = Image.new("RGB", (w, h), (15, 12, 20))
    px = img.load()
    for y in range(h):
        t = y / h
        r = int(20 + 40 * (1 - t))
        g = int(15 + 25 * (1 - t))
        b = int(35 + 60 * (1 - t))
        for x in range(w):
            n = rng.randint(-12, 12)
            px[x, y] = (max(0, r + n), max(0, g + n), max(0, b + n))
    return img.filter(ImageFilter.GaussianBlur(radius=2))


def fit_cover(img, w, h):
    return ImageOps.fit(img, (w, h), Image.LANCZOS)


def draw_centered(draw, text, font, y, fill, width=WIDTH):
    bbox = draw.textbbox((0, 0), text, font=font)
    tw = bbox[2] - bbox[0]
    draw.text(((width - tw) / 2, y), text, font=font, fill=fill)
    return bbox[3] - bbox[1]


def fetch_photo(seed_str):
    """Return path to a photo for this seed (cached). None on failure."""
    os.makedirs(PHOTO_CACHE, exist_ok=True)
    safe = slug(seed_str)
    path = os.path.join(PHOTO_CACHE, f"{safe}.jpg")
    if os.path.exists(path) and os.path.getsize(path) > 5000:
        return path
    url = f"https://picsum.photos/seed/{safe}/800/1400"
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "vhs-cover-gen/1.0"})
        with urllib.request.urlopen(req, timeout=20) as r, open(path, "wb") as f:
            f.write(r.read())
        # Validate it actually decodes as an image
        Image.open(path).verify()
        return path
    except Exception as e:
        print(f"  photo fetch failed for {safe}: {e}")
        if os.path.exists(path):
            os.remove(path)
        return None


# ---------------------------------------------------------------------------
# Render — shared helpers
# ---------------------------------------------------------------------------
def load_bg(title, photo_path):
    if photo_path and os.path.exists(photo_path):
        try:
            img = fit_cover(Image.open(photo_path).convert("RGB"), WIDTH, HEIGHT)
            return apply_photo_treatment(img, title)
        except Exception:
            pass
    return make_placeholder_bg(WIDTH, HEIGHT, seed=stable_hash(title) & 0xFFFF)


# ---------------------------------------------------------------------------
# Photo treatments — applied per-cover for visual variance
# ---------------------------------------------------------------------------
def tx_none(img, _seed):
    return img


def tx_bw(img, _seed):
    return img.convert("L").convert("RGB")


def tx_sepia(img, _seed):
    bw = img.convert("L")
    sepia = Image.merge("RGB", (
        bw.point(lambda v: min(255, int(v * 1.07))),
        bw.point(lambda v: int(v * 0.85)),
        bw.point(lambda v: int(v * 0.62)),
    ))
    return sepia


def tx_duotone(img, seed):
    """Map luminance to a gradient between two colors picked from a palette."""
    palettes = [
        ((20, 0, 60), (255, 80, 200)),    # dark purple -> hot pink
        ((0, 30, 60), (40, 220, 255)),    # navy -> cyan
        ((60, 0, 0), (255, 200, 80)),     # dark red -> gold
        ((0, 40, 20), (180, 255, 100)),   # dark green -> lime
        ((30, 0, 30), (255, 180, 40)),    # plum -> orange
    ]
    lo, hi = palettes[seed % len(palettes)]
    bw = img.convert("L")
    lut_r = [int(lo[0] + (hi[0] - lo[0]) * i / 255) for i in range(256)]
    lut_g = [int(lo[1] + (hi[1] - lo[1]) * i / 255) for i in range(256)]
    lut_b = [int(lo[2] + (hi[2] - lo[2]) * i / 255) for i in range(256)]
    return Image.merge("RGB", (
        bw.point(lut_r),
        bw.point(lut_g),
        bw.point(lut_b),
    ))


def tx_vignette(img, _seed):
    w, h = img.size
    mask = Image.new("L", (w, h), 0)
    md = ImageDraw.Draw(mask)
    for i in range(40):
        v = int(255 * (i / 40))
        md.ellipse([
            -i * 12, -i * 12,
            w + i * 12, h + i * 12
        ], fill=v)
    mask = mask.filter(ImageFilter.GaussianBlur(60))
    dark = Image.new("RGB", (w, h), (0, 0, 0))
    return Image.composite(img, dark, mask)


def tx_high_contrast(img, _seed):
    img = ImageEnhance.Contrast(img).enhance(1.6)
    img = ImageEnhance.Color(img).enhance(1.3)
    return img


# Weighted treatment pool — controls how often each appears
TREATMENTS = (
    [tx_none] * 5 +
    [tx_bw] * 2 +
    [tx_sepia] * 2 +
    [tx_duotone] * 3 +
    [tx_vignette] * 2 +
    [tx_high_contrast] * 2
)


def apply_photo_treatment(img, title):
    seed = stable_hash(title + ":tx")
    fn = TREATMENTS[seed % len(TREATMENTS)]
    return fn(img, seed)


def fit_title(title, font_name, max_width, max_size=130, min_size=50, step=6):
    """Find the largest font size at which `title` fits within max_width."""
    size = max_size
    font = find_font(font_name, size=size)
    dummy = ImageDraw.Draw(Image.new("RGB", (10, 10)))
    while size > min_size:
        bbox = dummy.textbbox((0, 0), title, font=font)
        if bbox[2] - bbox[0] <= max_width:
            return font, size
        size -= step
        font = find_font(font_name, size=size)
    return font, size


def draw_title_shadow(draw, title, font, x, y, color, shadow):
    draw.text((x + 4, y + 4), title, font=font, fill=shadow)
    draw.text((x, y), title, font=font, fill=color)


def draw_rating_box(draw, rating, x, y, color):
    rating_font = find_font("impact.ttf", size=44)
    rb = draw.textbbox((0, 0), rating, font=rating_font)
    pad = 12
    box_w = (rb[2] - rb[0]) + pad * 2
    box_h = (rb[3] - rb[1]) + pad * 2
    draw.rectangle([x, y, x + box_w, y + box_h], outline=color, width=4)
    draw.text((x + pad, y + pad - 4), rating, font=rating_font, fill=color)
    return box_w, box_h


def darken_band(canvas, x0, y0, x1, y1, alpha=255):
    overlay = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    ImageDraw.Draw(overlay).rectangle([x0, y0, x1, y1], fill=(0, 0, 0, alpha))
    return Image.alpha_composite(canvas.convert("RGBA"), overlay).convert("RGB")


def fade_band(canvas, top, bottom):
    """Vertical fade from transparent at `top` to opaque black at `bottom`."""
    overlay = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    span = bottom - top
    steps = 60
    for i in range(steps):
        a = int(255 * (i / steps))
        y = top + int(i * span / steps)
        od.rectangle([0, y, WIDTH, y + (span // steps + 1)], fill=(0, 0, 0, a))
    return Image.alpha_composite(canvas.convert("RGBA"), overlay).convert("RGB")


def finish(canvas):
    """Outer 6px black border."""
    ImageDraw.Draw(canvas).rectangle([0, 0, WIDTH - 1, HEIGHT - 1], outline=(0, 0, 0), width=6)
    return canvas


def draw_distributor_block(draw, distributor, runtime, y_dist, y_spec, color="white", title=""):
    dist_font = find_font("arialbd.ttf", size=22)
    spec_font = find_font("arial.ttf", size=14)
    draw_centered(draw, distributor, dist_font, y_dist, fill=color)
    spec = tech_strip(title, runtime)
    draw_centered(draw, spec, spec_font, y_spec, fill=(170, 170, 170))


def draw_banner_top(draw, banner, color, distributor=None):
    """PRESENTS line above the genre banner (if distributor provided)."""
    if distributor:
        presents_font = find_font("arialbd.ttf", size=15)
        draw_centered(draw, presents_line(distributor), presents_font, 14,
                      fill=(220, 220, 220))
        banner_font = find_font("arialbd.ttf", size=24)
        draw_centered(draw, banner, banner_font, 42, fill=color)
        draw_centered(draw, "* * *", find_font("arial.ttf", size=16), 78, fill=(180, 180, 180))
    else:
        banner_font = find_font("arialbd.ttf", size=26)
        draw_centered(draw, banner, banner_font, 36, fill=color)
        draw_centered(draw, "* * *", find_font("arial.ttf", size=18), 76, fill=(180, 180, 180))


def draw_tagline_block(draw, tagline, font_name, color, y_start, size=32):
    tag_font = find_font(font_name, size=size)
    y = y_start
    for line in tagline.split("\n"):
        h = draw_centered(draw, line, tag_font, y, fill=color)
        y += h + 8
    return y


# ---------------------------------------------------------------------------
# Layouts
# ---------------------------------------------------------------------------
def layout_bottom(canvas, title, tagline, banner, distributor, rating, runtime, style):
    """Photo full-bleed, title sits in lower third over a fade-to-black."""
    canvas = darken_band(canvas, 0, 0, WIDTH, 140)
    canvas = fade_band(canvas, HEIGHT - 600, HEIGHT - 360)
    canvas = darken_band(canvas, 0, HEIGHT - 360, WIDTH, HEIGHT)
    draw = ImageDraw.Draw(canvas)
    draw_banner_top(draw, banner, style["banner_color"], distributor=distributor)
    # STARRING line below banner
    draw_starring(draw, title, 30, 110, WIDTH - 60, (220, 220, 220))
    draw_rating_box(draw, rating, WIDTH - 110, 160, style["rating_box_color"])

    title_font, _ = fit_title(title, pick_font(style["title_pool"], title), WIDTH - 60)
    bbox = draw.textbbox((0, 0), title, font=title_font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    tx = (WIDTH - tw) / 2
    title_y = HEIGHT - 460 + (130 - th) // 2
    draw_title_shadow(draw, title, title_font, tx, title_y,
                      style["title_color"], style["title_shadow"])
    draw_tagline_block(draw, tagline, pick_font(TAGLINE_POOL, title + ":tagline"), style["tagline_color"], HEIGHT - 300)
    if has_blurb(title):
        draw_critic_blurb(draw, title, 40, HEIGHT - 175, WIDTH - 80, (240, 220, 160))
    draw_distributor_block(draw, distributor, runtime, HEIGHT - 110, HEIGHT - 70, title=title)
    return finish(canvas)


def layout_top_band(canvas, title, tagline, banner, distributor, rating, runtime, style):
    """Solid colored band at top holds the title; photo dominates middle; bottom black band."""
    band_h = 460
    bottom_h = 220
    overlay = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    od.rectangle([0, 0, WIDTH, band_h], fill=(*style["title_shadow"], 255))
    od.rectangle([0, HEIGHT - bottom_h, WIDTH, HEIGHT], fill=(0, 0, 0, 255))
    canvas = Image.alpha_composite(canvas.convert("RGBA"), overlay).convert("RGB")
    draw = ImageDraw.Draw(canvas)

    # PRESENTS line at very top of band
    draw_centered(draw, presents_line(distributor),
                  find_font("arialbd.ttf", size=15), 18, fill=(255, 255, 255, 220))
    # Banner inside top band
    draw_centered(draw, banner, find_font("arialbd.ttf", size=24), 46, fill=style["banner_color"])
    draw_centered(draw, "* * *", find_font("arial.ttf", size=16), 80, fill=(220, 220, 220))

    # Title inside top band
    title_font, _ = fit_title(title, pick_font(style["title_pool"], title), WIDTH - 60, max_size=140)
    bbox = draw.textbbox((0, 0), title, font=title_font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    tx = (WIDTH - tw) / 2
    ty = 130 + ((band_h - 130) - th) // 2 - 30
    draw_title_shadow(draw, title, title_font, tx, ty,
                      style["title_color"], (0, 0, 0))

    # STARRING just below the title, inside the band
    draw_starring(draw, title, 30, ty + th + 28, WIDTH - 60, (240, 240, 240))

    # Rating in top-right of band
    draw_rating_box(draw, rating, WIDTH - 110, band_h - 90, style["rating_box_color"])

    # Tagline + critic blurb + distributor in bottom band
    tag_y = HEIGHT - bottom_h + 16
    end_y = draw_tagline_block(draw, tagline, pick_font(TAGLINE_POOL, title + ":tagline"), style["tagline_color"], tag_y, size=26)
    if has_blurb(title):
        draw_critic_blurb(draw, title, 40, HEIGHT - 130, WIDTH - 80, (240, 220, 160))
    draw_distributor_block(draw, distributor, runtime, HEIGHT - 60, HEIGHT - 32, title=title)
    return finish(canvas)


def layout_framed(canvas, title, tagline, banner, distributor, rating, runtime, style):
    """Photo as inset frame in upper half; title in solid block below."""
    # Black background
    bg = canvas
    canvas = Image.new("RGB", (WIDTH, HEIGHT), (10, 10, 10))
    # Inset photo frame
    frame_x0, frame_y0 = 50, 100
    frame_x1, frame_y1 = WIDTH - 50, 720
    photo_inset = bg.crop((0, 0, WIDTH, HEIGHT)).resize(
        (frame_x1 - frame_x0 - 24, frame_y1 - frame_y0 - 24), Image.LANCZOS
    )
    canvas.paste(photo_inset, (frame_x0 + 12, frame_y0 + 12))
    draw = ImageDraw.Draw(canvas)
    draw.rectangle([frame_x0, frame_y0, frame_x1, frame_y1],
                   outline=style["banner_color"], width=12)

    # PRESENTS + banner above frame
    draw_centered(draw, presents_line(distributor),
                  find_font("arialbd.ttf", size=15), 24, fill=(220, 220, 220))
    draw_centered(draw, banner, find_font("arialbd.ttf", size=24), 56, fill=style["banner_color"])

    # Rating top-right of frame
    draw_rating_box(draw, rating, frame_x1 - 100, frame_y0 + 10, style["rating_box_color"])

    # Title in middle on dark area (no fade needed; it's black)
    title_font, _ = fit_title(title, pick_font(style["title_pool"], title), WIDTH - 60, max_size=130)
    bbox = draw.textbbox((0, 0), title, font=title_font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    tx = (WIDTH - tw) / 2
    ty = 770
    draw_title_shadow(draw, title, title_font, tx, ty,
                      style["title_color"], style["title_shadow"])

    # STARRING just below title
    draw_starring(draw, title, 30, ty + th + 24, WIDTH - 60, (220, 220, 220))

    # Tagline lower
    draw_tagline_block(draw, tagline, pick_font(TAGLINE_POOL, title + ":tagline"),
                       style["tagline_color"], 980)
    if has_blurb(title):
        draw_critic_blurb(draw, title, 40, HEIGHT - 175, WIDTH - 80, (240, 220, 160))

    # Distributor at bottom
    draw_distributor_block(draw, distributor, runtime, HEIGHT - 110, HEIGHT - 70, title=title)
    return finish(canvas)


def layout_split(canvas, title, tagline, banner, distributor, rating, runtime, style):
    """Photo top half, solid black bottom half with all text."""
    # Crop photo to top half
    bg_top = canvas.crop((0, 0, WIDTH, 760))
    canvas = Image.new("RGB", (WIDTH, HEIGHT), "black")
    canvas.paste(bg_top, (0, 0))
    # Soft fade at the seam
    overlay = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    for i in range(40):
        a = int(255 * (i / 40))
        od.rectangle([0, 720 + i * 2, WIDTH, 720 + (i + 1) * 2], fill=(0, 0, 0, a))
    canvas = Image.alpha_composite(canvas.convert("RGBA"), overlay).convert("RGB")
    draw = ImageDraw.Draw(canvas)

    # PRESENTS + banner small at very top (overlay on photo)
    draw_centered(draw, presents_line(distributor),
                  find_font("arialbd.ttf", size=14), 14, fill=(255, 255, 255))
    draw_centered(draw, banner, find_font("arialbd.ttf", size=22), 36,
                  fill=style["banner_color"])
    # Rating top-right
    draw_rating_box(draw, rating, WIDTH - 110, 70, style["rating_box_color"])

    # STARRING line right above the title-area split
    draw_starring(draw, title, 30, 770, WIDTH - 60, (220, 220, 220))

    # Title in upper portion of black bottom half
    title_font, _ = fit_title(title, pick_font(style["title_pool"], title), WIDTH - 60, max_size=140)
    bbox = draw.textbbox((0, 0), title, font=title_font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    tx = (WIDTH - tw) / 2
    ty = 820
    draw_title_shadow(draw, title, title_font, tx, ty,
                      style["title_color"], style["title_shadow"])

    # Decorative rule under title
    draw.rectangle([60, ty + th + 30, WIDTH - 60, ty + th + 36],
                   fill=style["banner_color"])

    # Tagline
    draw_tagline_block(draw, tagline, pick_font(TAGLINE_POOL, title + ":tagline"), style["tagline_color"], ty + th + 70)

    if has_blurb(title):
        draw_critic_blurb(draw, title, 40, HEIGHT - 175, WIDTH - 80, (240, 220, 160))

    # Distributor at very bottom
    draw_distributor_block(draw, distributor, runtime, HEIGHT - 110, HEIGHT - 70, title=title)
    return finish(canvas)


def layout_diagonal(canvas, title, tagline, banner, distributor, rating, runtime, style):
    """Photo full-bleed with a diagonal colored slash carrying the title at angle."""
    canvas = darken_band(canvas, 0, 0, WIDTH, HEIGHT, alpha=70)  # subtle photo wash
    angle_deg = -14
    # Build the diagonal band as a separate RGBA layer
    band_layer = Image.new("RGBA", (WIDTH * 2, 280), (0, 0, 0, 0))
    bd = ImageDraw.Draw(band_layer)
    bd.rectangle([0, 0, WIDTH * 2, 280], fill=(*style["banner_color"], 235))
    band_layer = band_layer.rotate(angle_deg, resample=Image.BICUBIC, expand=True)
    bx = (WIDTH - band_layer.width) // 2
    by = HEIGHT // 2 - band_layer.height // 2
    canvas_rgba = canvas.convert("RGBA")
    canvas_rgba.alpha_composite(band_layer, (bx, by))

    # Title rendered to its own image, then rotated
    title_font, _ = fit_title(title, pick_font(style["title_pool"], title), int(WIDTH * 1.3), max_size=160)
    tmp = Image.new("RGBA", (WIDTH * 2, 260), (0, 0, 0, 0))
    td = ImageDraw.Draw(tmp)
    bbox = td.textbbox((0, 0), title, font=title_font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    td.text(((tmp.width - tw) // 2 + 5, (tmp.height - th) // 2 + 5),
            title, font=title_font, fill=style["title_shadow"])
    td.text(((tmp.width - tw) // 2, (tmp.height - th) // 2),
            title, font=title_font, fill=style["title_color"])
    tmp = tmp.rotate(angle_deg, resample=Image.BICUBIC, expand=True)
    tx = (WIDTH - tmp.width) // 2
    ty = HEIGHT // 2 - tmp.height // 2
    canvas_rgba.alpha_composite(tmp, (tx, ty))
    canvas = canvas_rgba.convert("RGB")
    draw = ImageDraw.Draw(canvas)

    # PRESENTS + banner top-left
    draw.text((40, 18), presents_line(distributor),
              font=find_font("arialbd.ttf", size=14), fill=(220, 220, 220))
    draw.text((40, 42), banner, font=find_font("arialbd.ttf", size=22),
              fill=style["banner_color"])
    # STARRING below banner
    draw_starring(draw, title, 40, 76, WIDTH - 80, (235, 235, 235), align="left")
    # Rating bottom-left
    draw_rating_box(draw, rating, 40, HEIGHT - 220, style["rating_box_color"])
    # Tagline lower right
    tag_font = find_font(pick_font(TAGLINE_POOL, title + ":tagline"), size=28)
    y = HEIGHT - 200
    for line in tagline.split("\n"):
        bbox = draw.textbbox((0, 0), line, font=tag_font)
        tw_line = bbox[2] - bbox[0]
        draw.text((WIDTH - 40 - tw_line, y), line, font=tag_font, fill=style["tagline_color"])
        y += bbox[3] - bbox[1] + 6
    if has_blurb(title):
        draw_critic_blurb(draw, title, 40, HEIGHT - 130, WIDTH - 80, (240, 220, 160), align="right")
    # Distributor at bottom (small, right-aligned)
    dist_font = find_font("arialbd.ttf", size=20)
    bbox = draw.textbbox((0, 0), distributor, font=dist_font)
    draw.text((WIDTH - 40 - (bbox[2] - bbox[0]), HEIGHT - 80),
              distributor, font=dist_font, fill="white")
    spec_font = find_font("arial.ttf", size=14)
    spec = tech_strip(title, runtime)
    bbox = draw.textbbox((0, 0), spec, font=spec_font)
    draw.text((WIDTH - 40 - (bbox[2] - bbox[0]), HEIGHT - 50),
              spec, font=spec_font, fill=(170, 170, 170))
    return finish(canvas)


def layout_side_stripe(canvas, title, tagline, banner, distributor, rating, runtime, style):
    """Vertical colored stripe on the left with title rotated 90° going up the stripe."""
    stripe_w = 240
    overlay = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    od.rectangle([0, 0, stripe_w, HEIGHT], fill=(*style["title_shadow"], 255))
    canvas = Image.alpha_composite(canvas.convert("RGBA"), overlay).convert("RGB")
    draw = ImageDraw.Draw(canvas)

    # PRESENTS + banner across very top of photo area
    draw.text((stripe_w + 30, 28), presents_line(distributor),
              font=find_font("arialbd.ttf", size=14), fill=(220, 220, 220))
    draw.text((stripe_w + 30, 52), banner,
              font=find_font("arialbd.ttf", size=22), fill=style["banner_color"])
    # STARRING below banner in the photo area
    draw_starring(draw, title, stripe_w + 30, 88, WIDTH - stripe_w - 60,
                  (235, 235, 235), align="left")

    # Vertical title rendered rotated
    title_font, _ = fit_title(title, pick_font(style["title_pool"], title), HEIGHT - 200, max_size=130)
    tmp = Image.new("RGBA", (HEIGHT, stripe_w), (0, 0, 0, 0))
    td = ImageDraw.Draw(tmp)
    bbox = td.textbbox((0, 0), title, font=title_font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    tx = (tmp.width - tw) // 2
    ty = (tmp.height - th) // 2 - 8
    td.text((tx + 4, ty + 4), title, font=title_font, fill=(0, 0, 0))
    td.text((tx, ty), title, font=title_font, fill=style["title_color"])
    rotated = tmp.rotate(90, resample=Image.BICUBIC, expand=True)
    canvas_rgba = canvas.convert("RGBA")
    canvas_rgba.alpha_composite(rotated, (0, 0))
    canvas = canvas_rgba.convert("RGB")
    draw = ImageDraw.Draw(canvas)

    # Rating top-right
    draw_rating_box(draw, rating, WIDTH - 110, 40, style["rating_box_color"])

    # Tagline + blurb bottom of photo area
    canvas = darken_band(canvas, stripe_w, HEIGHT - 280, WIDTH, HEIGHT, alpha=200)
    draw = ImageDraw.Draw(canvas)
    tag_font = find_font(pick_font(TAGLINE_POOL, title + ":tagline"), size=26)
    y = HEIGHT - 260
    for line in tagline.split("\n"):
        bbox = draw.textbbox((0, 0), line, font=tag_font)
        draw.text((stripe_w + 30, y), line, font=tag_font, fill=style["tagline_color"])
        y += bbox[3] - bbox[1] + 6
    if has_blurb(title):
        draw_critic_blurb(draw, title, stripe_w + 30, HEIGHT - 130,
                          WIDTH - stripe_w - 60, (240, 220, 160), align="left")
    # Distributor + tech strip bottom-right
    dist_font = find_font("arialbd.ttf", size=20)
    bbox = draw.textbbox((0, 0), distributor, font=dist_font)
    draw.text((WIDTH - 30 - (bbox[2] - bbox[0]), HEIGHT - 70),
              distributor, font=dist_font, fill="white")
    spec_font = find_font("arial.ttf", size=14)
    spec = tech_strip(title, runtime)
    bbox = draw.textbbox((0, 0), spec, font=spec_font)
    draw.text((WIDTH - 30 - (bbox[2] - bbox[0]), HEIGHT - 40),
              spec, font=spec_font, fill=(170, 170, 170))
    return finish(canvas)


def layout_solid(canvas, title, tagline, banner, distributor, rating, runtime, style):
    """No photo. Solid color background, big title, geometric ornament."""
    canvas = Image.new("RGB", (WIDTH, HEIGHT), style["title_shadow"])
    draw = ImageDraw.Draw(canvas)

    # Decorative concentric rectangle frame
    inset = 40
    for i, t in enumerate([12, 4, 4]):
        col = style["banner_color"] if i % 2 == 0 else style["title_color"]
        draw.rectangle([inset, inset, WIDTH - inset, HEIGHT - inset],
                       outline=col, width=t)
        inset += t + 14

    # PRESENTS + banner at top
    draw_centered(draw, presents_line(distributor),
                  find_font("arialbd.ttf", size=15), 110, fill=style["title_color"])
    draw_centered(draw, banner, find_font("arialbd.ttf", size=24), 142,
                  fill=style["banner_color"])
    # STARRING below banner
    draw_starring(draw, title, 80, 178, WIDTH - 160, style["title_color"])

    # Big title centered
    title_font, _ = fit_title(title, pick_font(style["title_pool"], title), WIDTH - 200, max_size=160)
    bbox = draw.textbbox((0, 0), title, font=title_font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    tx = (WIDTH - tw) / 2
    ty = (HEIGHT - th) / 2 - 60
    draw_title_shadow(draw, title, title_font, tx, ty,
                      style["title_color"], style["banner_color"])

    # Decorative line
    draw.rectangle([200, ty + th + 50, WIDTH - 200, ty + th + 56],
                   fill=style["banner_color"])

    # Tagline below
    draw_tagline_block(draw, tagline, pick_font(TAGLINE_POOL, title + ":tagline"), style["tagline_color"],
                       ty + th + 90, size=30)
    if has_blurb(title):
        draw_critic_blurb(draw, title, 80, HEIGHT - 220, WIDTH - 160, style["title_color"])

    # Rating top-right
    draw_rating_box(draw, rating, WIDTH - 130, 130, style["rating_box_color"])

    # Distributor at bottom
    draw_distributor_block(draw, distributor, runtime, HEIGHT - 130, HEIGHT - 95,
                           color=style["title_color"], title=title)
    return finish(canvas)


def layout_corner(canvas, title, tagline, banner, distributor, rating, runtime, style):
    """Photo full-bleed, title left-aligned in a colored block in the upper-left."""
    canvas = darken_band(canvas, 0, 0, WIDTH, HEIGHT, alpha=80)
    draw = ImageDraw.Draw(canvas)

    # Title block in upper-left
    block_x0, block_y0 = 0, 80
    title_font, ts = fit_title(title, pick_font(style["title_pool"], title), 700, max_size=110)
    bbox = draw.textbbox((0, 0), title, font=title_font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    block_x1 = tw + 80
    block_y1 = block_y0 + th + 60
    overlay = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    od.rectangle([block_x0, block_y0, block_x1, block_y1],
                 fill=(*style["title_shadow"], 240))
    canvas = Image.alpha_composite(canvas.convert("RGBA"), overlay).convert("RGB")
    draw = ImageDraw.Draw(canvas)

    # PRESENTS + banner above the title block, left-aligned
    draw.text((40, 18), presents_line(distributor),
              font=find_font("arialbd.ttf", size=14), fill=(220, 220, 220))
    draw.text((40, 42), banner, font=find_font("arialbd.ttf", size=22),
              fill=style["banner_color"])

    # Title left-aligned inside block
    draw.text((44, block_y0 + 30), title, font=title_font, fill=style["title_color"])

    # STARRING below title block
    draw_starring(draw, title, 40, block_y1 + 16, WIDTH - 80,
                  (235, 235, 235), align="left")

    # Tagline lower-left, with darken under it
    canvas = darken_band(canvas, 0, HEIGHT - 280, WIDTH, HEIGHT, alpha=200)
    draw = ImageDraw.Draw(canvas)
    tag_font = find_font(pick_font(TAGLINE_POOL, title + ":tagline"), size=28)
    y = HEIGHT - 260
    for line in tagline.split("\n"):
        bbox = draw.textbbox((0, 0), line, font=tag_font)
        draw.text((40, y), line, font=tag_font, fill=style["tagline_color"])
        y += bbox[3] - bbox[1] + 6
    if has_blurb(title):
        draw_critic_blurb(draw, title, 40, HEIGHT - 130, WIDTH - 80,
                          (240, 220, 160), align="left")

    # Rating top-right
    draw_rating_box(draw, rating, WIDTH - 110, 30, style["rating_box_color"])

    # Distributor + tech strip bottom-right
    dist_font = find_font("arialbd.ttf", size=20)
    bbox = draw.textbbox((0, 0), distributor, font=dist_font)
    draw.text((WIDTH - 40 - (bbox[2] - bbox[0]), HEIGHT - 70),
              distributor, font=dist_font, fill="white")
    spec_font = find_font("arial.ttf", size=14)
    spec = tech_strip(title, runtime)
    bbox = draw.textbbox((0, 0), spec, font=spec_font)
    draw.text((WIDTH - 40 - (bbox[2] - bbox[0]), HEIGHT - 40),
              spec, font=spec_font, fill=(170, 170, 170))
    return finish(canvas)


LAYOUTS = [
    layout_bottom, layout_top_band, layout_framed, layout_split,
    layout_diagonal, layout_side_stripe, layout_solid, layout_corner,
]


def render_cover(title, tagline, genre_banner, distributor, rating, runtime,
                 photo_path=None, output_path=None):
    style = DISTRIBUTOR_STYLES.get(distributor, DISTRIBUTOR_STYLES[DEFAULT_STYLE_KEY])
    bg = load_bg(title, photo_path)
    canvas = Image.new("RGB", (WIDTH, HEIGHT), "black")
    canvas.paste(bg, (0, 0))

    layout = LAYOUTS[stable_hash(title) % len(LAYOUTS)]
    canvas = layout(canvas, title, tagline, genre_banner, distributor, rating, runtime, style)

    # Burst sticker as final overlay for ~45% of covers. Position varies by hash.
    if has_burst(title):
        positions = [
            (WIDTH - 120, HEIGHT - 480),    # mid-right
            (130, HEIGHT - 480),            # mid-left
            (WIDTH - 120, 320),             # upper-right (below rating)
            (130, 320),                     # upper-left
        ]
        cx, cy = positions[stable_hash(title + ":burstpos") % len(positions)]
        burst_palette = [
            ((255, 220, 60), (180, 0, 0)),    # yellow + red text
            ((220, 30, 30), (255, 255, 255)), # red + white text
            ((255, 255, 255), (200, 0, 0)),   # white + red text
            ((255, 100, 0), (255, 255, 255)), # orange + white text
        ]
        bp = burst_palette[stable_hash(title + ":burstcol") % len(burst_palette)]
        angle = -25 + (stable_hash(title + ":burstang") % 50)
        canvas = draw_burst_sticker(canvas, burst_text(title), cx, cy,
                                    fill_color=bp[0], text_color=bp[1],
                                    angle=angle, radius=95)

    if output_path:
        canvas.save(output_path)
    return canvas


# ---------------------------------------------------------------------------
# Modes
# ---------------------------------------------------------------------------
def run_single(photo_arg=None):
    out = os.path.join(HERE, "cover_preview.png")
    title, tagline, banner, dist, rating, runtime = COVERS[0]
    photo_path = photo_arg or fetch_photo(title)
    render_cover(title, tagline, banner, dist, rating, runtime,
                 photo_path=photo_path, output_path=out)
    print(f"Wrote {out}")


def run_batch():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    os.makedirs(PHOTO_CACHE, exist_ok=True)

    report_pool_sizes()
    print(f"Fetching {len(COVERS)} photos in parallel...")
    titles = [c[0] for c in COVERS]
    with ThreadPoolExecutor(max_workers=12) as ex:
        list(ex.map(fetch_photo, titles))

    print(f"Rendering {len(COVERS)} covers...")
    for i, (title, tagline, banner, dist, rating, runtime) in enumerate(COVERS, 1):
        photo_path = os.path.join(PHOTO_CACHE, f"{slug(title)}.jpg")
        if not os.path.exists(photo_path):
            photo_path = None
        out = os.path.join(OUTPUT_DIR, f"{slug(title)}.png")
        render_cover(title, tagline, banner, dist, rating, runtime,
                     photo_path=photo_path, output_path=out)
        if i % 10 == 0:
            print(f"  {i}/{len(COVERS)}")
    print(f"Done. Output: {OUTPUT_DIR}")


if __name__ == "__main__":
    if "--batch" in sys.argv:
        run_batch()
    else:
        photo = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else None
        run_single(photo)

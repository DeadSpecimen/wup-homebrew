"""Where things are: wup-menu's own folders, and libwup's (the SDK the menu is built with: next to this folder, or
$LIBWUP). Every host script imports this instead of building paths itself; libwup's tools, imported from here, get
this module as their `paths` too, so it carries all of libwup's names as well."""
import importlib.util, os, sys

WUPMENU = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIBWUP = os.environ.get('LIBWUP') or os.path.join(os.path.dirname(WUPMENU), 'libwup')
_lib = os.path.join(LIBWUP, 'tools', 'paths.py')
if not os.path.isfile(_lib):
    raise SystemExit('libwup not found at %s: clone it next to wup-menu, or set LIBWUP' % LIBWUP)
_spec = importlib.util.spec_from_file_location('libwup_paths', _lib)
_mod = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(_mod)
globals().update({k: v for k, v in vars(_mod).items() if not k.startswith('__')})   # ROOT, INCLUDE, SDK, out(), ...
sys.path.append(os.path.join(LIBWUP, 'tools'))                                       # wupbuild, wupack, wupmon, ...

MENU = os.path.join(WUPMENU, 'menu')                 # the Homebrew Menu (an app like the others)
BOOT = os.path.join(WUPMENU, 'boot')                 # the hook and the loader
FIRMWARE = os.environ.get('WUP_FIRMWARE') or os.path.join(os.path.dirname(WUPMENU), 'wup-firmware')   # your pad's dumps (never in git)


def local_out(*parts):
    """a directory under wup-menu's build/, created if needed"""
    d = os.path.join(WUPMENU, 'build', *parts)
    os.makedirs(d, exist_ok=True)
    return d

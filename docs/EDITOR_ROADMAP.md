# Hamun Editor: Blender-Based Editor Integration Roadmap
# نقشهٔ راه ادیتور مبتنی بر سورس Blender برای هامون

> **Status:** Blender source audit started; integration branch created  
> **Branch:** `feature/blender-source-integration`  
> **Engine status:** Pre-Alpha  
>
> **وضعیت:** بررسی سورس Blender آغاز شده و شاخهٔ یکپارچه‌سازی ساخته شده است  
> **شاخه:** `feature/blender-source-integration`  
> **وضعیت موتور:** پیش‌آلفا

## Vision | چشم‌انداز

Use the **actual Blender source code and editor architecture as the foundation** for a Hamun-branded authoring application. Preserve Blender's proven modeling, animation, viewport, and content-authoring systems while customizing the visual identity and selected interaction details (especially Move, Rotate, and Scale gizmos, toolbars, icons, panels, and workspace defaults). Integrate Hamun engine capabilities through a deliberately designed bridge rather than pretending that a UI reskin automatically replaces Blender's scene or renderer.

از **سورس واقعی Blender و معماری ادیتور آن به‌عنوان پایه** استفاده شود و یک برنامهٔ تولید محتوا با هویت بصری هامون ساخته شود. ابزارهای تثبیت‌شدهٔ مدل‌سازی، انیمیشن، ویوپورت و تولید محتوا حفظ شوند؛ در عین حال ظاهر و جزئیات تعامل، به‌ویژه دستگیره‌های Move/Rotate/Scale، نوار ابزار، آیکون‌ها، پنل‌ها و چیدمان اولیه تغییر کنند. قابلیت‌های موتور هامون از طریق یک لایهٔ ارتباطی مشخص یکپارچه شوند؛ تغییر پوسته به‌تنهایی جایگزینی صحنه یا رندرر Blender محسوب نمی‌شود.

## Engineering principles | اصول مهندسی

- Extend the existing Hamun architecture instead of creating a second engine.
- Keep editor tools separate from runtime code where practical.
- Use shared engine data models for scenes, meshes, materials, skeletons, and animation.
- Prefer a coherent, integrated implementation over disconnected demo panels.
- Keep changes on this feature branch until builds and tests pass.
- Preserve clear provenance and license notices for every third-party dependency.
- Keep Blender-derived code in a clearly identified Blender-based source tree/repository and preserve required copyright and GPL notices. Review distribution and integration boundaries before release; do not assume the existing MIT license automatically covers Blender-derived code.

- معماری فعلی هامون گسترش پیدا کند، نه اینکه موتور دومی ساخته شود.
- ابزارهای ادیتور تا حد امکان از کد اجرایی بازی جدا باشند.
- صحنه، مش، متریال، اسکلت و انیمیشن از مدل‌های داده‌ای مشترک موتور استفاده کنند.
- به‌جای پنل‌های نمایشیِ جدا از هم، یک پیاده‌سازی منسجم و کاربردی ساخته شود.
- تغییرات تا عبور از ساخت و تست‌ها در همین شاخه باقی بمانند.
- منشأ و مجوز تمام وابستگی‌های جانبی ثبت شود.
- کد مشتق‌شده از Blender در درخت/مخزن مشخص و جداگانه نگهداری شود و اعلان‌های حق نشر و مجوز GPL حفظ شوند. پیش از انتشار، مرزهای یکپارچه‌سازی و توزیع بررسی شوند؛ مجوز MIT فعلی هامون به‌طور خودکار مجوز کدهای مشتق‌شده از Blender نیست.

## Existing foundation | زیرساخت فعلی

The current editor target is `Tools/HamunEditor`. Its known components include:

- `src/main.cpp`: current editor application entry point and UI implementation.
- `src/EditorAssetIndex.cpp/.hpp`: editor asset indexing.
- `src/EditorSceneDocument.cpp/.hpp`: scene-document handling.
- `src/EditorScenePreview.cpp/.hpp`: scene preview.
- `tests/SceneDocumentTests.cpp`: existing scene-document tests.
- `Tools/HamunEditor/CMakeLists.txt`: editor target and test wiring.

ادیتور فعلی در مسیر `Tools/HamunEditor` قرار دارد. اجزای شناخته‌شده:

- `src/main.cpp`: نقطهٔ ورود برنامه و پیاده‌سازی فعلی رابط ادیتور.
- `src/EditorAssetIndex.cpp/.hpp`: نمایه‌سازی دارایی‌ها.
- `src/EditorSceneDocument.cpp/.hpp`: مدیریت سند صحنه.
- `src/EditorScenePreview.cpp/.hpp`: پیش‌نمایش صحنه.
- `tests/SceneDocumentTests.cpp`: تست‌های موجود برای سند صحنه.
- `Tools/HamunEditor/CMakeLists.txt`: تنظیمات هدف ادیتور و تست‌ها.

## Work plan | برنامهٔ اجرایی

### 1. Blender source audit and distribution model | بررسی سورس Blender و مدل انتشار

- Track the official `blender/blender` repository and pin a tested upstream commit before making patches.
- Identify the View3D gizmo, transform tool, theme, workspace, and editor registration code paths.
- Keep the first UI patch narrow: Hamun-branded workspace defaults and custom transform-gizmo styling without removing Blender's underlying tool behavior.
- Choose an explicit source strategy: a fork or a separately tracked Blender source checkout. Do not copy isolated GPL files into the existing MIT engine tree.
- Review GPL obligations and any engine-bridge architecture before distributing binaries or source.

- مخزن رسمی `blender/blender` دنبال و پیش از اعمال تغییرات، یک commit مشخص برای مبنای آزمایش ثبت شود.
- مسیرهای کد دستگیره‌های View3D، ابزارهای Transform، پوسته، Workspace و ثبت ادیتورها شناسایی شوند.
- اولین تغییر رابط محدود باشد: چیدمان اولیه و هویت بصری هامون و ظاهر اختصاصی دستگیره‌ها، بدون حذف رفتار اصلی ابزارهای Blender.
- روش نگهداری سورس روشن باشد: فورک یا checkout جداگانه و قابل‌ردیابی از Blender؛ فایل‌های GPL به‌صورت پراکنده در درخت فعلی موتور MIT کپی نشوند.
- پیش از انتشار سورس یا فایل اجرایی، الزامات GPL و معماری پل ارتباطی با موتور بررسی شوند.

### 2. Editor foundation | زیرساخت ادیتور

- Establish a dockable/workspace-friendly editor layout and a dedicated 3D viewport.
- Add reliable selection, scene hierarchy, transform controls, undo/redo, and editor command routing.
- Keep scene edits serializable and covered by automated tests.

- چیدمان قابل‌گسترش پنل‌ها و ویوپورت سه‌بعدی اختصاصی ایجاد شود.
- انتخاب اشیا، سلسله‌مراتب صحنه، ابزارهای جابه‌جایی/چرخش/مقیاس، واگرد/ازنو و مدیریت فرمان‌ها پیاده‌سازی شوند.
- تغییرات صحنه قابلیت ذخیره‌سازی داشته باشند و تست خودکار آن‌ها را پوشش دهد.

### 3. Mesh authoring | مدل‌سازی و ویرایش مش

- Define editable mesh data, topology operations, and a safe edit/commit workflow.
- Add core operations such as vertex/edge/face selection, extrude, inset, merge, and basic transforms.
- Validate geometry and preserve stable asset identifiers.

- ساختار دادهٔ مش قابل‌ویرایش، عملیات توپولوژی و گردش‌کار امن ویرایش/ثبت تغییرات تعریف شود.
- انتخاب رأس/یال/وجه، اکسترود، اینست، ادغام و تبدیل‌های پایه اضافه شوند.
- اعتبار هندسه بررسی و شناسه‌های پایدار دارایی‌ها حفظ شوند.

### 4. UV and materials | یووی و متریال

- Add UV channel inspection/editing and a UV preview/editor.
- Integrate material assignment and a material preview with the existing renderer.
- Keep imported and native assets traceable and reproducible.

- مشاهده و ویرایش کانال‌های UV و ابزار پیش‌نمایش/ویرایش UV اضافه شوند.
- تخصیص متریال و پیش‌نمایش آن با رندرر موجود یکپارچه شود.
- منشأ و قابلیت بازتولید دارایی‌های واردشده و بومی حفظ شود.

### 5. Skeletons and animation | اسکلت و انیمیشن

- Define skeleton, skinning, animation-clip, and timeline data structures.
- Add a skeletal hierarchy view, keyframe/timeline tools, and animation preview.
- Verify that authored assets can be used by the Hamun runtime.

- ساختار دادهٔ اسکلت، اسکینینگ، کلیپ انیمیشن و تایم‌لاین تعریف شود.
- نمایش سلسله‌مراتب استخوان‌ها، ابزارهای کلیدفریم/تایم‌لاین و پیش‌نمایش انیمیشن اضافه شوند.
- استفاده از دارایی‌های ساخته‌شده در بخش اجرایی هامون آزمایش شود.

### 6. Asset interoperability | سازگاری با فرمت‌های دارایی

- Define native asset formats and versioning.
- Prioritize practical import/export paths such as glTF, with explicit validation and error reporting.
- Do not promise support for every Blender feature or file format until it is implemented and tested.

- فرمت‌های بومی دارایی و نسخه‌بندی آن‌ها تعریف شوند.
- مسیرهای کاربردی ورود/خروج، از جمله glTF، با اعتبارسنجی و گزارش خطای روشن در اولویت قرار گیرند.
- پشتیبانی از همهٔ قابلیت‌ها یا فرمت‌های Blender تا زمان پیاده‌سازی و تست واقعی ادعا نشود.

## Definition of done | معیار پایان کار

A feature is considered complete only when it:
1. works in the Hamun editor rather than in a mock panel;
2. persists correctly in a project/asset;
3. integrates with the renderer or runtime where relevant;
4. has automated tests for core behavior;
5. builds through the repository's documented CMake workflow; and
6. includes English and Persian documentation where user-facing.

هر قابلیت فقط زمانی کامل محسوب می‌شود که:
۱. در ادیتور واقعی هامون کار کند، نه در یک پنل نمایشی؛
۲. در پروژه یا دارایی به‌درستی ذخیره شود؛
۳. در صورت نیاز با رندرر یا بخش اجرایی یکپارچه باشد؛
۴. برای رفتارهای اصلی تست خودکار داشته باشد؛
۵. از روند ساخت CMake مستندشدهٔ مخزن عبور کند؛
۶. برای بخش‌های قابل‌مشاهدهٔ کاربر مستندات انگلیسی و فارسی داشته باشد.

## Current status | وضعیت فعلی

The dedicated branch `feature/blender-source-integration` is based on the Hamun repository's current main branch. The official Blender repository is accessible and a View3D navigation-gizmo source file has been inspected. This is an initial source audit only: Blender has not been vendored, compiled, or merged into Hamun, and no modified executable has been built yet.

شاخهٔ اختصاصی `feature/blender-source-integration` از شاخهٔ فعلی main هامون ساخته شده است. مخزن رسمی Blender در دسترس است و یکی از فایل‌های سورس دستگیره‌های ناوبری View3D بررسی شده است. این مرحله فقط بررسی اولیهٔ سورس است: هنوز سورس Blender به مخزن اضافه، کامپایل یا با هامون ادغام نشده و فایل اجرایی تغییر‌یافته‌ای ساخته نشده است.

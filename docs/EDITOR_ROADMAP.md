# Hamun Editor: Blender-Inspired Editor Roadmap
# نقشهٔ راه ویرایشگر هامون با الهام از Blender

> **Status:** Planning and architecture audit started  
> **Branch:** `feature/blender-inspired-editor`  
> **Engine status:** Pre-Alpha  
>
> **وضعیت:** برنامه‌ریزی و بررسی معماری آغاز شده است  
> **شاخه:** `feature/blender-inspired-editor`  
> **وضعیت موتور:** پیش‌آلفا

## Vision | چشم‌انداز

Build a **native, independent 3D editor inside Hamun Game Engine** with a powerful modeling and content-authoring workflow inspired by Blender. Blender must not be required to run Hamun, and the editor must not merely embed Blender's window or launch Blender as a hidden dependency.

ساخت یک **ویرایشگر سه‌بعدی بومی و مستقل در داخل موتور بازی هامون** با گردش‌کار قدرتمند مدل‌سازی و تولید محتوا، با الهام از Blender. برای اجرای هامون نباید به نصب Blender نیاز باشد؛ همچنین هدف صرفاً جاسازی پنجرهٔ Blender یا اجرای پنهانی آن نیست.

## Engineering principles | اصول مهندسی

- Extend the existing Hamun architecture instead of creating a second engine.
- Keep editor tools separate from runtime code where practical.
- Use shared engine data models for scenes, meshes, materials, skeletons, and animation.
- Prefer a coherent, integrated implementation over disconnected demo panels.
- Keep changes on this feature branch until builds and tests pass.
- Preserve clear provenance and license notices for every third-party dependency.
- Do not copy Blender GPL code into the MIT-licensed Hamun codebase unless the project explicitly chooses and satisfies a compatible licensing strategy. Reusing ideas is not the same as copying implementation.

- معماری فعلی هامون گسترش پیدا کند، نه اینکه موتور دومی ساخته شود.
- ابزارهای ادیتور تا حد امکان از کد اجرایی بازی جدا باشند.
- صحنه، مش، متریال، اسکلت و انیمیشن از مدل‌های داده‌ای مشترک موتور استفاده کنند.
- به‌جای پنل‌های نمایشیِ جدا از هم، یک پیاده‌سازی منسجم و کاربردی ساخته شود.
- تغییرات تا عبور از ساخت و تست‌ها در همین شاخه باقی بمانند.
- منشأ و مجوز تمام وابستگی‌های جانبی ثبت شود.
- کد GPL بلندر بدون تصمیم‌گیری روشن دربارهٔ مجوز و رعایت کامل الزامات آن وارد کد MIT هامون نشود. الهام‌گرفتن از ایده‌ها با کپی‌کردن پیاده‌سازی یکسان نیست.

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

### 1. Architecture and license audit | بررسی معماری و مجوز

- Map the current editor UI, scene/world representation, renderer interfaces, asset pipeline, and build/test setup.
- Identify missing foundations and define the editor/runtime boundary.
- Review Blender source organization and license obligations before considering code reuse.
- Record an implementation decision: independent implementation, compatible third-party components, or an explicitly GPL-compatible distribution model.

- رابط فعلی ادیتور، مدل صحنه و جهان، رابط‌های رندرر، خط لولهٔ دارایی و روند ساخت و تست مستند شوند.
- زیرساخت‌های ناقص و مرز میان ادیتور و بخش اجرایی مشخص شوند.
- ساختار سورس Blender و الزامات مجوز آن پیش از هرگونه استفاده از کد بررسی شود.
- تصمیم استفاده از پیاده‌سازی مستقل، اجزای جانبی سازگار، یا مدل انتشار سازگار با GPL ثبت شود.

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

The roadmap is established on a dedicated feature branch. The first implementation gate is the source audit and a build/test baseline; no Blender code has been merged and no end-to-end modeling feature is claimed complete yet.

نقشهٔ راه در یک شاخهٔ توسعهٔ اختصاصی ثبت شده است. نخستین مرحله، بررسی سورس و ثبت وضعیت پایهٔ ساخت و تست‌هاست؛ هنوز کدی از Blender ادغام نشده و هیچ قابلیت کامل مدل‌سازی به‌عنوان خروجی نهایی اعلام نمی‌شود.

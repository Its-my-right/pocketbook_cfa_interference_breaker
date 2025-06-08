local ffi = require("ffi")
local inkview = ffi.load("inkview")
require("ffi/inkview_h")
local logger = require("logger")
local framebuffer = require("ffi/framebuffer_pocketbook")
local Device = require("device")
local PowerD = Device.powerd

-- Paramétrage BREAK RAINBOW
local param_radius_min = 9999
local param_radius_max_diviser = 2.4

-- Chargement des bibliothèques partagées
local moire = ffi.load("custom_libs/moire_filter_fftw_eco.so")
local color_detect = ffi.load("custom_libs/color_detect.so")

local fft_initialized = false  -- Variable globale pour savoir si fft_module_init() a été appelée

ffi.cdef[[
    void remove_moire(unsigned char *fb_data, int width, int height, int line_length, float param_radius_min, float param_radius_max_diviser);
]]

ffi.cdef[[
    bool is_framebuffer_colored(uint8_t* data, int width, int height, int stride, int tolerance);
]]

ffi.cdef[[
    int init_moire_resources();
]]

ffi.cdef[[
    void cleanup_moire_resources();
]]


-- Appel de la fonction sur le framebuffer
local function remove_moire_on_fb(fb)
	local fb_data = fb.data
	local width = fb._vinfo.width
	local height = fb._vinfo.height
	local line_length  = fb._finfo.line_length
    moire.remove_moire(fb_data, width, height, line_length, param_radius_min, param_radius_max_diviser)
end

-- Fonction qui analyse un framebuffer pour détecter la présence de couleur
local function framebuffer_has_color(fb, tolerance)
    -- Valeur de tolérance par défaut
    tolerance = tolerance or 20  -- Valeur par défaut identique au code original
    -- Appel de la fonction C avec les données du framebuffer
    local is_colored = color_detect.is_framebuffer_colored(
        fb.data,
        fb._vinfo.width,
        fb._vinfo.height,
        fb._finfo.line_length,
        tolerance
    )
	
    if not is_colored and not fft_initialized then
        moire.init_moire_resources()
        fft_initialized = true
    end
	
	return is_colored
end


-- CODE EXECUTE A LA FERMETURE DE KOREADER
local original_exit = Device.exit
function Device:exit()
    --print("KOReader se ferme - exécution du code de nettoyage")
	
	if fft_initialized then
        moire.cleanup_moire_resources()
		fft_initialized = false
    end
	
    -- Puis appeler la fonction originale pour permettre la fermeture normale
    return original_exit(self)
end

-- CODE EXECUTE A LA MISE EN VEILLE DE LA LISEUSE
local original_beforeSuspend = PowerD.beforeSuspend
function PowerD:beforeSuspend()
    --print("La liseuse passe en veille - exécution du code de nettoyage")
	
	if fft_initialized then
        moire.cleanup_moire_resources()
		fft_initialized = false
    end
	
    -- Puis appeler la fonction originale pour permettre la mise en veille normale
    return original_beforeSuspend(self)
end

local function _adjustAreaColours(fb)
    if fb.device.hasColorScreen() then
        fb.debug("adjusting image color saturation")

        inkview.adjustAreaDefault(fb.data, fb._finfo.line_length, fb._vinfo.width, fb._vinfo.height)
    end
end

local function _adjustAreaBW(fb)		
    fb.debug("adjusting image BW")
	remove_moire_on_fb(fb)
end

local function _updateFull(fb, x, y, w, h, dither)
    fb.debug("refresh: inkview full", x, y, w, h, dither, fb.device.hasColorScreen(), fb.device)
	
	if (dither and framebuffer_has_color(fb, 20)) then
		_adjustAreaColours(fb)
	else
		_adjustAreaBW(fb)
    end

    if fb.device.hasColorScreen() then
        inkview.FullUpdateHQ()
    else
        inkview.FullUpdate()
    end
end

local function _updatePartial(fb, x, y, w, h, dither, hq)
    -- Use "hq" argument to trigger high quality refresh for color Pocketbook devices.
	x, y, w, h = _getPhysicalRect(fb, x, y, w, h)

    fb.debug("refresh: inkview partial", x, y, w, h, dither)

    if (dither and framebuffer_has_color(fb, 20)) then
		_adjustAreaColours(fb)
	else
		_adjustAreaBW(fb)
    end

    if fb.device.hasColorScreen() and hq then
        inkview.PartialUpdateHQ(x, y, w, h)
    else
        inkview.PartialUpdate(x, y, w, h)
    end
end

local function _updateFast(fb, x, y, w, h, dither)
	x, y, w, h = _getPhysicalRect(fb, x, y, w, h)

    fb.debug("refresh: inkview fast", x, y, w, h, dither)

    if (dither and framebuffer_has_color(fb, 20)) then
		_adjustAreaColours(fb)
	else
		_adjustAreaBW(fb)
    end

    inkview.DynamicUpdate(x, y, w, h)
end

function _getPhysicalRect(fb, x, y, w, h)
    local bb = fb.full_bb or fb.bb
    x, y, w, h = bb:getBoundedRect(x, y, w, h)
    return bb:getPhysicalRect(x, y, w, h)
end

function framebuffer:refreshPartialImp(x, y, w, h, dither)
	_updatePartial(self, x, y, w, h, dither, false)
end

function framebuffer:refreshFlashPartialImp(x, y, w, h, dither)
	_updatePartial(self, x, y, w, h, dither, true)
end

function framebuffer:refreshUIImp(x, y, w, h, dither)
	_updatePartial(self, x, y, w, h, dither, false)
end

function framebuffer:refreshFlashUIImp(x, y, w, h, dither)
	_updatePartial(self, x, y, w, h, dither, true)
end

function framebuffer:refreshFullImp(x, y, w, h, dither)
   _updateFull(self, x, y, w, h, dither)
end

function framebuffer:refreshFastImp(x, y, w, h, dither)
	_updateFast(self, x, y, w, h, dither)
end
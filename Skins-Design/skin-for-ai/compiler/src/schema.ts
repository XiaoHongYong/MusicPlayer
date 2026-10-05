import { z } from "zod";

export const STATE_NAMES = [
    "normal",
    "hover",
    "pressed",
    "disabled",
    "focused",
    "selected",
    "checked",
    "indeterminate",
] as const;

export const LAYER_TYPES = [
    "rect",
    "roundedRect",
    "circle",
    "path",
    "line",
    "image",
    "text",
    "gradient",
    "shadow",
    "blur",
    "mask",
    "clip",
    "border",
    "highlight",
] as const;

export const COMPONENT_TYPES = [
    "windowBackground",
    "windowFrame",
    "captionToolbar",
    "captionBar",
    "button",
    "toggleButton",
    "slider",
    "checkbox",
    "edit",
    "comboBox",
    "menu",
    "albumArt",
    "panel",
    "searchBar",
    "scrollbar",
    "toolbar",
    "iconStrip",
    "tabButton",
    "playlist",
    "icon",
    "sprite",
] as const;

export type ComponentType = (typeof COMPONENT_TYPES)[number];

const hexColor = z.string().regex(/^#([0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$/, "颜色须为 #RRGGBB 或 #RRGGBBAA");
const ref = z.string().regex(/^\$/, "引用须以 $ 开头");
export const colorValue = z.union([hexColor, ref]);

export const GradientSchema = z.object({
    type: z.enum(["linear", "radial"]),
    angle: z.number().optional(),
    stops: z.array(z.tuple([z.number(), z.string()])),
}).strict();

export const FillSchema = z.union([colorValue, GradientSchema]);

export const ShadowSchema = z.object({
    offset: z.tuple([z.number(), z.number()]).optional(),
    blur: z.number().optional(),
    spread: z.number().optional(),
    color: colorValue.optional(),
}).strict();

export const TypographySchema = z.object({
    font: z.string().optional(),
    size: z.number().optional(),
    weight: z.number().optional(),
}).strict();

export const LayerSchema: z.ZodType<Record<string, unknown>> = z.lazy(() =>
    z.object({
        type: z.enum(LAYER_TYPES),
    }).passthrough()
);

export const MaterialSchema = z.object({
    fill: FillSchema.optional(),
    blur: z.number().optional(),
    border: z.object({
        width: z.number(),
        color: colorValue,
    }).strict().optional(),
    noise: z.number().optional(),
    highlight: z.object({ opacity: z.number() }).strict().optional(),
}).strict();

export const AssetSchema = z.object({
    type: z.enum(["svg", "image", "mask"]),
    source: z.string(),
}).strict();

export const StateVisualSchema = z.object({
    $extends: z.string().optional(),
    material: z.string().optional(),
    fill: FillSchema.optional(),
    border: z.object({ width: z.number(), color: colorValue, radius: z.union([z.number(), ref]).optional() }).passthrough().optional(),
    shadow: z.union([ShadowSchema, ref]).optional(),
    textColor: colorValue.optional(),
    iconColor: colorValue.optional(),
    opacity: z.number().optional(),
    scale: z.number().optional(),
    offset: z.tuple([z.number(), z.number()]).optional(),
    layers: z.array(LayerSchema).optional(),
    radius: z.union([z.number(), ref]).optional(),
}).passthrough();

export const MetaSchema = z.object({
    name: z.string().min(1),
    version: z.string().optional(),
    author: z.string().optional(),
    description: z.string().optional(),
    extraResourceFolder: z.string().optional(),
}).strict();

export const TokensSchema = z.object({
    colors: z.record(z.string(), colorValue).optional(),
    dimensions: z.record(z.string(), z.number()).optional(),
    radius: z.record(z.string(), z.number()).optional(),
    typography: z.record(z.string(), TypographySchema).optional(),
    shadows: z.record(z.string(), ShadowSchema).optional(),
}).strict();

export const LayoutChildSchema: z.ZodType<{
    control: string;
    id?: string;
    name?: string;
    component?: string;
    rect: string;
    children?: unknown[];
}> = z.object({
    control: z.string(),
    id: z.string().optional(),
    name: z.string().optional(),
    component: z.string().optional(),
    rect: z.string(),
    children: z.array(z.lazy(() => LayoutChildSchema)).optional(),
}).passthrough();

export const LayoutSchema = z.object({
    window: z.object({
        width: z.number(),
        height: z.number(),
        minWidth: z.number().optional(),
        minHeight: z.number().optional(),
    }).strict(),
    children: z.array(LayoutChildSchema),
}).strict();

export const LyricsSchema = z.object({
    textColor: colorValue.optional(),
    highlightColor: colorValue.optional(),
    bgColor: colorValue.optional(),
    floatingBgColor: colorValue.optional(),
    fontSize: z.number().optional(),
}).strict();

export const ExportsSchema = z.object({
    scale: z.array(z.number()).optional(),
    format: z.enum(["png", "webp"]).optional(),
    button: z.object({ sizes: z.array(z.tuple([z.number(), z.number()])) }).optional(),
    window: z.object({ size: z.tuple([z.number(), z.number()]) }).optional(),
}).passthrough();

export const SkinJsonSchema = z.object({
    schema: z.literal("skin.v1"),
    meta: MetaSchema,
    tokens: TokensSchema.default({}),
    materials: z.record(z.string(), MaterialSchema).optional(),
    assets: z.record(z.string(), AssetSchema).optional(),
    templates: z.record(z.string(), z.array(LayerSchema)).optional(),
    components: z.record(z.string(), z.unknown()),
    layout: LayoutSchema.optional(),
    lyrics: LyricsSchema.optional(),
    exports: ExportsSchema.optional(),
}).strict();

export type SkinJson = z.infer<typeof SkinJsonSchema>;

export function assertComponentType(type: unknown, path: string): ComponentType {
    if (typeof type !== "string" || !(COMPONENT_TYPES as readonly string[]).includes(type)) {
        throw new Error(`${path}: 未知组件 type "${String(type)}"，允许: ${COMPONENT_TYPES.join(", ")}`);
    }
    return type as ComponentType;
}

export function formatZodError(err: z.ZodError): string {
    return err.issues.map((i) => `  ${i.path.join(".") || "<root>"}: ${i.message}`).join("\n");
}

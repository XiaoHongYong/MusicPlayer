import { isHexColor } from "./color.js";
import { LAYER_TYPES, STATE_NAMES } from "./schema.js";

export interface ResolveIssue {
    path: string;
    message: string;
    level: "error" | "warn";
}

export interface ResolvedSkin {
    raw: Record<string, unknown>;
    tokens: Record<string, unknown>;
    materials: Record<string, unknown>;
    assets: Record<string, unknown>;
    templates: Record<string, unknown>;
    components: Record<string, Record<string, unknown>>;
    issues: ResolveIssue[];
}

function lookup(root: Record<string, unknown>, dotted: string): unknown {
    const parts = dotted.split(".");
    let cur: unknown = root;
    for (const p of parts) {
        if (cur && typeof cur === "object" && p in (cur as object)) {
            cur = (cur as Record<string, unknown>)[p];
        } else {
            return undefined;
        }
    }
    return cur;
}

export function resolveRef(expr: string, bag: Record<string, unknown>, path: string): unknown {
    if (!expr.startsWith("$")) {
        return expr;
    }
    const body = expr.slice(1);
    const value = lookup(bag, body);
    if (value === undefined) {
        throw new Error(`${path}: 无法解析引用 ${expr}`);
    }
    return value;
}

function walk(value: unknown, bag: Record<string, unknown>, path: string, issues: ResolveIssue[]): unknown {
    if (typeof value === "string") {
        if (value.startsWith("$")) {
            const resolved = resolveRef(value, bag, path);
            if (typeof resolved === "string" && resolved.startsWith("$")) {
                return walk(resolved, bag, path, issues);
            }
            return resolved;
        }
        return value;
    }
    if (Array.isArray(value)) {
        return value.map((v, i) => walk(v, bag, `${path}[${i}]`, issues));
    }
    if (value && typeof value === "object") {
        const out: Record<string, unknown> = {};
        for (const [k, v] of Object.entries(value as Record<string, unknown>)) {
            if (k === "$extends") {
                out[k] = v;
                continue;
            }
            out[k] = walk(v, bag, `${path}.${k}`, issues);
        }
        return out;
    }
    return value;
}

function mergeShallow(base: Record<string, unknown>, over: Record<string, unknown>): Record<string, unknown> {
    return { ...base, ...over };
}

function resolveStates(states: Record<string, unknown>, path: string, issues: ResolveIssue[]): Record<string, Record<string, unknown>> {
    const names = Object.keys(states);
    for (const name of names) {
        if (!(STATE_NAMES as readonly string[]).includes(name)) {
            issues.push({ path: `${path}.${name}`, message: `非法状态名 "${name}"`, level: "error" });
        }
    }
    const resolved: Record<string, Record<string, unknown>> = {};
    const visiting = new Set<string>();

    const resolveOne = (name: string): Record<string, unknown> => {
        if (resolved[name]) {
            return resolved[name];
        }
        const raw = states[name];
        if (!raw || typeof raw !== "object") {
            return {};
        }
        if (visiting.has(name)) {
            throw new Error(`${path}.${name}: $extends 循环引用`);
        }
        visiting.add(name);
        const obj = { ...(raw as Record<string, unknown>) };
        const parent = obj.$extends;
        delete obj.$extends;
        let base: Record<string, unknown> = {};
        if (typeof parent === "string") {
            if (!(parent in states)) {
                throw new Error(`${path}.${name}: $extends 指向未定义状态 "${parent}"`);
            }
            base = resolveOne(parent);
        }
        visiting.delete(name);
        resolved[name] = mergeShallow(base, obj);
        return resolved[name];
    };

    for (const name of names) {
        resolveOne(name);
    }
    return resolved;
}

function applyMaterial(state: Record<string, unknown>, materials: Record<string, unknown>): Record<string, unknown> {
    const matRef = state.material;
    if (typeof matRef !== "string") {
        return state;
    }
    // 已解析的 material 可能已是对象
    return state;
}

function mergeMaterialObject(state: Record<string, unknown>): Record<string, unknown> {
    const mat = state.material;
    if (!mat || typeof mat !== "object") {
        return state;
    }
    const m = mat as Record<string, unknown>;
    const out = { ...state };
    if (out.fill === undefined && m.fill !== undefined) {
        out.fill = m.fill;
    }
    if (out.border === undefined && m.border !== undefined) {
        out.border = m.border;
    }
    if (out.blur === undefined && m.blur !== undefined) {
        out.blur = m.blur;
    }
    if (out.noise === undefined && m.noise !== undefined) {
        out.noise = m.noise;
    }
    if (out.highlight === undefined && m.highlight !== undefined) {
        out.highlight = m.highlight;
    }
    delete out.material;
    return out;
}

export function resolveSkin(skin: {
    tokens?: unknown;
    materials?: unknown;
    assets?: unknown;
    templates?: unknown;
    components: Record<string, unknown>;
}): ResolvedSkin {
    const issues: ResolveIssue[] = [];
    const bag: Record<string, unknown> = {
        tokens: skin.tokens ?? {},
        materials: skin.materials ?? {},
        assets: skin.assets ?? {},
        templates: skin.templates ?? {},
    };

    const tokens = walk(bag.tokens, bag, "tokens", issues) as Record<string, unknown>;
    const materials = walk(bag.materials, { ...bag, tokens }, "materials", issues) as Record<string, unknown>;
    const assets = walk(bag.assets, { ...bag, tokens, materials }, "assets", issues) as Record<string, unknown>;
    const templates = walk(bag.templates, { ...bag, tokens, materials, assets }, "templates", issues) as Record<string, unknown>;
    const bag2 = { tokens, materials, assets, templates };

    const components: Record<string, Record<string, unknown>> = {};
    for (const [name, comp] of Object.entries(skin.components)) {
        if (!comp || typeof comp !== "object") {
            throw new Error(`components.${name}: 必须是对象`);
        }
        const walked = walk(comp, bag2, `components.${name}`, issues) as Record<string, unknown>;
        if (typeof walked.template === "string") {
            const tpl = lookup(bag2, walked.template.startsWith("$") ? walked.template.slice(1) : `templates.${walked.template}`);
            if (Array.isArray(tpl) && walked.layers === undefined) {
                walked.layers = tpl;
            }
        }
        mergeStateMap(walked, `components.${name}.states`, issues);
        const thumb = walked.thumb as Record<string, unknown> | undefined;
        if (thumb) {
            mergeStateMap(thumb, `components.${name}.thumb.states`, issues);
        }
        components[name] = walked;
        void applyMaterial;
    }

    assertNoDanglingRefs({ tokens, materials, assets, templates, components }, issues);
    void isHexColor;

    return {
        raw: { tokens, materials, assets, templates, components },
        tokens, materials, assets, templates, components, issues,
    };
}

function assertNoDanglingRefs(value: unknown, issues: ResolveIssue[], path = ""): void {
    if (typeof value === "string" && value.startsWith("$") && path.indexOf("$extends") < 0) {
        issues.push({ path, message: `未解析的引用 ${value}`, level: "error" });
        return;
    }
    if (Array.isArray(value)) {
        value.forEach((v, i) => assertNoDanglingRefs(v, issues, `${path}[${i}]`));
        return;
    }
    if (value && typeof value === "object") {
        for (const [k, v] of Object.entries(value as Record<string, unknown>)) {
            if (k === "$extends") {
                continue;
            }
            assertNoDanglingRefs(v, issues, path ? `${path}.${k}` : k);
        }
    }
}

function mergeStateMap(owner: Record<string, unknown>, path: string, issues: ResolveIssue[]): void {
    if (!owner.states || typeof owner.states !== "object") {
        return;
    }
    const st = resolveStates(owner.states as Record<string, unknown>, path, issues);
    const merged: Record<string, Record<string, unknown>> = {};
    for (const [k, v] of Object.entries(st)) {
        merged[k] = mergeMaterialObject(v);
    }
    owner.states = merged;
}

export function validateLayerTypes(layers: unknown, path: string, issues: ResolveIssue[]): void {
    if (!Array.isArray(layers)) {
        return;
    }
    layers.forEach((layer, i) => {
        if (!layer || typeof layer !== "object") {
            issues.push({ path: `${path}[${i}]`, message: "layer 必须是对象", level: "error" });
            return;
        }
        const t = (layer as Record<string, unknown>).type;
        if (typeof t !== "string" || !(LAYER_TYPES as readonly string[]).includes(t)) {
            issues.push({ path: `${path}[${i}].type`, message: `未知 layer type "${String(t)}"`, level: "error" });
        }
        const nested = (layer as Record<string, unknown>).layers;
        if (nested) {
            validateLayerTypes(nested, `${path}[${i}].layers`, issues);
        }
    });
}

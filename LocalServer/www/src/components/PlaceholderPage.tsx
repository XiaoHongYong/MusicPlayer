export function PlaceholderPage({ title, note }: { title: string; note: string }) {
  return (
    <div className="p-6">
      <h1 className="text-2xl font-semibold">{title}</h1>
      <p className="mt-3 max-w-xl text-sm text-muted-foreground">{note}</p>
    </div>
  );
}

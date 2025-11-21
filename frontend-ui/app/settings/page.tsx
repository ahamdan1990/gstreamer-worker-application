'use client';

import { Card, CardContent } from '@/components/ui/card';
import { Settings as SettingsIcon } from 'lucide-react';
import { Header } from '@/components/header';

export default function SettingsPage() {
  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100 dark:from-slate-950 dark:to-slate-900">
      <Header title="Settings" description="System configuration" />
      <main className="container mx-auto px-6 py-8">
        <Card>
          <CardContent className="flex flex-col items-center justify-center py-16">
            <SettingsIcon className="h-16 w-16 text-muted-foreground mb-4" />
            <h3 className="text-xl font-semibold mb-2">Settings Coming Soon</h3>
            <p className="text-sm text-muted-foreground">
              Configure system preferences, notifications, and integrations
            </p>
          </CardContent>
        </Card>
      </main>
    </div>
  );
}

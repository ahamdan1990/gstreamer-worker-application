'use client';

import { Card, CardContent } from '@/components/ui/card';
import { FileText } from 'lucide-react';
import { Header } from '@/components/header';

export default function ReportsPage() {
  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100 dark:from-slate-950 dark:to-slate-900">
      <Header title="Reports" description="Generate and view reports" />
      <main className="container mx-auto px-6 py-8">
        <Card>
          <CardContent className="flex flex-col items-center justify-center py-16">
            <FileText className="h-16 w-16 text-muted-foreground mb-4" />
            <h3 className="text-xl font-semibold mb-2">Reports Coming Soon</h3>
            <p className="text-sm text-muted-foreground">
              Export motion events, camera logs, and system reports
            </p>
          </CardContent>
        </Card>
      </main>
    </div>
  );
}

// Programme de test (test.cmd) : fait tourner les vrais calculs du plugin sur des corps simules, hors du jeu.
// Inclus a la fin de main.cpp quand RR_TEST est defini. Verifie les signes et la stabilite des muscles, de la
// retenue et des pas. Il n'y a ni articulations ni decor : le sol est un plan ideal sous les pieds d'appui.
// Ca ne remplace pas le test en jeu.
namespace
{
	struct FakeBody { alignas(16) UInt8 mem[0x220]; };

	void InitBody(FakeBody& f, V3 pos, float mass, float inertia)
	{
		memset(f.mem, 0, sizeof(f.mem));
		At<UInt8>(f.mem, hk::kMotionType) = hk::kBox;
		float* r = &At<float>(f.mem, hk::kRotation);
		r[0] = 1.f; r[5] = 1.f; r[10] = 1.f; // colonnes (1,0,0) (0,1,0) (0,0,1), 4 floats par colonne
		float* c = &At<float>(f.mem, hk::kCenterOfMass);
		c[0] = pos.x; c[1] = pos.y; c[2] = pos.z;
		float* ii = &At<float>(f.mem, hk::kInvInertia);
		ii[0] = ii[1] = ii[2] = 1.f / inertia;
		At<float>(f.mem, hk::kInvMass) = 1.f / mass;
	}

	void SetRot(FakeBody& f, const M3& m)
	{
		for (int c = 0; c < 3; ++c)
		{
			float* col = &At<float>(f.mem, hk::kRotation + 0x10 * c);
			col[0] = m.c[c].x; col[1] = m.c[c].y; col[2] = m.c[c].z;
		}
	}

	M3 AxisAngle(V3 axis, float a)
	{
		axis = Norm(axis);
		M3 m;
		V3 e[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
		for (int c = 0; c < 3; ++c) // Rodrigues
			m.c[c] = e[c] * cosf(a) + Cross(axis, e[c]) * sinf(a) + axis * (Dot(axis, e[c]) * (1.f - cosf(a)));
		return m;
	}

	// un pas de temps : position et rotation suivent les vitesses (vitesse angulaire dans le repere du monde)
	void Integrate(FakeBody& f, float dt)
	{
		float* c = &At<float>(f.mem, hk::kCenterOfMass);
		V3 v = Load3(f.mem, hk::kLinVelocity), w = Load3(f.mem, hk::kAngVelocity);
		c[0] += v.x * dt; c[1] += v.y * dt; c[2] += v.z * dt;
		float a = Len(w) * dt;
		if (a > 1e-7f) SetRot(f, Mul(AxisAngle(w, a), RotOf(f.mem)));
	}

	int g_failed = 0;
	void Check(bool ok, const char* what, float value)
	{
		printf("%s  %-58s %g\n", ok ? "ok    " : "ECHEC ", what, value);
		if (!ok) ++g_failed;
	}

	// humain simplifie : bassin, poitrine, deux mollets, deux pieds. h0 = 10, pieds ecartes de 3.
	struct Dummy
	{
		FakeBody body[6];
		Body b[6];
		MuscleTask m;
		Dummy()
		{
			const char* names[6] = {"Bip01 Pelvis", "Bip01 Spine2", "Bip01 L Calf", "Bip01 R Calf", "Bip01 L Foot", "Bip01 R Foot"};
			V3 pos[6] = {{0, 0, 10}, {0, 0, 14}, {-1.5f, 0, 4}, {1.5f, 0, 4}, {-1.5f, 0, 0}, {1.5f, 0, 0}};
			float mass[6] = {15, 35, 4, 4, 1.5f, 1.5f};
			int parent[6] = {-1, 0, 0, 0, 2, 3};
			for (int k = 0; k < 6; ++k)
			{
				InitBody(body[k], pos[k], mass[k], mass[k]);
				b[k] = {names[k], body[k].mem, parent[k]};
			}
			memset(&m, 0, sizeof(m));
			m.h0 = 10.f;
			m.stanceWidth = 3.f;
			m.stepFoot = -1;
			m.fallDir = {0, 1, 0};
			m.legPower[0] = m.legPower[1] = 1.f;
		}
		// une image : controleur, gravite, sol ideal sous les pieds d'appui, integration
		bool Step(float t, float dt, float vital, const char*& why)
		{
			bool touched[kMaxBodies] = {};
			bool ok = BalanceHumanoid(m, b, 6, 1, 0, t, dt, vital, (DWORD)(1000 + t * 1000.f), touched, why);
			for (int k = 0; k < 6; ++k)
			{
				AddLinVel(body[k].mem, V3{0, 0, -9.8f * dt});
				bool leg = k >= 2;
				int side = (k - 2) % 2;
				float* v = &At<float>(body[k].mem, hk::kLinVelocity);
				if (leg && side != m.stepFoot) v[0] = v[1] = v[2] = 0.f; // jambe d'appui : tenue par le sol
				Integrate(body[k], dt);
			}
			return ok;
		}
		float Height() { return BodyCenter(body[0].mem).z; }
	};
}

int main()
{
	const float dt = 1.f / 60.f;
	const float T0 = sqrtf(10.f / 9.8f); // temps naturel du mannequin (h0 = 10, g = 9,8)

	// 1. rotation -> axe x angle
	V3 rv = RotVec(AxisAngle({0, 0, 1}, 0.5f));
	Check(fabsf(rv.z - 0.5f) < 1e-4f && fabsf(rv.x) + fabsf(rv.y) < 1e-4f, "RotVec : 0,5 rad autour de z", rv.z);

	// 2. muscle d'articulation : l'os revient vers sa pose par rapport a un parent incline, lui-meme libre
	{
		FakeBody parent, child;
		InitBody(parent, {0, 0, 0}, 30.f, 30.f);
		InitBody(child, {0, 0, 1}, 3.f, 3.f);
		M3 rp = AxisAngle({1, 2, 3}, 0.7f), rel = AxisAngle({-2, 1, 0.5f}, 0.4f);
		SetRot(parent, rp);
		SetRot(child, Mul(Mul(rp, rel), AxisAngle({0, 1, 1}, 1.2f))); // 1,2 rad d'ecart avec sa pose
		float worst = 0.f;
		for (int i = 0; i < 180; ++i)
		{
			JointMuscle(child.mem, parent.mem, rel, {0, 0, 0}, 30.f, 6.f, 8.f, dt);
			Integrate(parent, dt);
			Integrate(child, dt);
			float w = Len(Load3(child.mem, hk::kAngVelocity));
			if (w > worst) worst = w;
		}
		float err = Len(RotVec(Mul(Mul(RotOf(parent.mem), rel), Tr(RotOf(child.mem)))));
		Check(err < 0.05f, "muscle d'articulation : ecart restant apres 3 s (rad)", err);
		Check(worst < 20.f, "muscle d'articulation : vitesse maximale (rad/s)", worst);
	}

	// 3. ressort entre deux os (main, pied) : la distance a la cible tombe a presque rien
	{
		Dummy d;
		V3 target = {3, 2, 6};
		for (int i = 0; i < 120; ++i)
		{
			PullBody(d.b, 4, 0, target, 300.f, 35.f, 10.f, 1.f, dt);
			Integrate(d.body[4], dt);
			Integrate(d.body[0], dt);
		}
		float left = Len(target - BodyCenter(d.body[4].mem));
		Check(left < 0.3f, "ressort : distance a la cible apres 2 s", left);
	}

	// 4. geometrie
	V3 turn = TurnTo({1, 0, 0}, {0, 1, 0}, 3.14159f);
	Check(fabsf(turn.z - 1.5708f) < 1e-3f, "TurnTo : x vers y = quart de tour autour de z", turn.z);
	float capped = Len(TurnTo({1, 0, 0}, {-1, 0.01f, 0}, 1.f));
	Check(fabsf(capped - 1.f) < 1e-3f, "TurnTo : limite a 1 rad", capped);
	V3 cl;
	float dist = DistToSegmentXY({0, 5, 0}, {-2, 0, 0}, {2, 0, 0}, cl);
	Check(fabsf(dist - 5.f) < 1e-4f && fabsf(cl.x) < 1e-4f, "distance a un segment", dist);

	// 5. debout : parti 10 % trop bas, il remonte a sa hauteur, sans pas et sans perdre l'equilibre
	{
		Dummy d;
		for (int k = 0; k < 2; ++k) At<float>(d.body[k].mem, hk::kCenterOfMass + 8) -= 1.f;
		const char* why = "";
		bool ok = true;
		float low = 100.f, high = 0.f;
		for (int i = 0; i < 180 && ok; ++i)
		{
			ok = d.Step(i * dt, dt, 1.f, why);
			if (i > 90) { low = d.Height() < low ? d.Height() : low; high = d.Height() > high ? d.Height() : high; }
		}
		Check(ok, "debout : garde l'equilibre 3 s", ok ? 1.f : 0.f);
		Check(fabsf(low - 10.f) < 0.3f && fabsf(high - 10.f) < 0.3f, "debout : hauteur du bassin (attendue 10)", d.Height());
		Check(d.m.steps == 0, "debout : aucun pas", (float)d.m.steps);
	}

	// 6. petite poussee : il se retient sans faire de pas et revient au-dessus de ses pieds
	{
		Dummy d;
		for (int k = 0; k < 2; ++k) At<float>(d.body[k].mem, hk::kLinVelocity + 4) = 0.08f * 10.f / T0;
		const char* why = "";
		bool ok = true;
		for (int i = 0; i < 300 && ok; ++i) ok = d.Step(i * dt, dt, 1.f, why);
		Check(ok && d.m.steps == 0, "petite poussee : se retient sans pas", (float)d.m.steps);
		Check(fabsf(BodyCenter(d.body[0].mem).y) < 0.5f, "petite poussee : revenu au-dessus de ses pieds (y)", BodyCenter(d.body[0].mem).y);
	}

	// 7. forte poussee : il fait un pas, du bon cote (vers +y)
	{
		Dummy d;
		for (int k = 0; k < 2; ++k) At<float>(d.body[k].mem, hk::kLinVelocity + 4) = 0.6f * 10.f / T0;
		const char* why = "";
		bool ok = true;
		float footY = 0.f;
		for (int i = 0; i < 90 && ok; ++i)
		{
			ok = d.Step(i * dt, dt, 1.f, why);
			for (int f = 4; f < 6; ++f) footY = BodyCenter(d.body[f].mem).y > footY ? BodyCenter(d.body[f].mem).y : footY;
		}
		Check(d.m.steps >= 1, "forte poussee : fait au moins un pas", (float)d.m.steps);
		Check(footY > 2.f, "forte poussee : le pied part du cote ou il tombe (y du pied)", footY);
	}

	// 8. sans forces : ses jambes plient et il perd l'equilibre
	{
		Dummy d;
		const char* why = "";
		bool ok = true;
		int i = 0;
		for (; i < 240 && ok; ++i) ok = d.Step(i * dt, dt, 0.2f, why);
		Check(!ok, "sans forces : il tombe (secondes)", i * dt);
		printf("        raison : %s\n", why);
	}

	// 9. jambe touchee : il s'affaisse un peu mais tient sur l'autre
	{
		Dummy d;
		d.m.legPower[0] = g_cfg.woundedLimb;
		const char* why = "";
		bool ok = true;
		for (int i = 0; i < 120 && ok; ++i) ok = d.Step(i * dt, dt, 1.f, why);
		Check(ok && d.Height() < 9.95f && d.Height() > 8.f, "jambe touchee : il s'affaisse un peu mais tient", d.Height());
	}

	// Pour information (pas un test) : ce qu'il fait selon la force de la poussee, sur 5 secondes
	printf("\nPoussee (x h0/T0)  ->  resultat sur 5 s\n");
	for (float push : {0.1f, 0.2f, 0.4f, 0.6f, 0.9f, 1.3f, 1.8f})
	{
		Dummy d;
		for (int k = 0; k < 2; ++k) At<float>(d.body[k].mem, hk::kLinVelocity + 4) = push * 10.f / T0;
		const char* why = "";
		bool ok = true;
		int i = 0;
		for (; i < 300 && ok; ++i) ok = d.Step(i * dt, dt, 1.f, why);
		printf("  %.1f  ->  %d pas, %s%s, bassin a y = %.1f\n", push, d.m.steps, ok ? "reste debout" : "tombe : ", ok ? "" : why,
			BodyCenter(d.body[0].mem).y);
	}

	printf(g_failed ? "\n%d ECHEC(S)\n" : "\nTous les tests passent.\n", g_failed);
	return g_failed;
}
